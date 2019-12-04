// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.printing.PrintShareActivity;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.qrcode.QrCodeShareActivity;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.ui_metrics.CanonicalURLResult;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.GURLUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of share interface. Mostly a wrapper around ShareSheetCoordinator.
 */
public class ShareDelegateImpl implements ShareDelegate {
    private BottomSheetController mBottomSheetController;
    private final ShareSheetDelegate mDelegate;
    static final String CANONICAL_URL_RESULT_HISTOGRAM = "Mobile.CanonicalURLResult";
    private static boolean sScreenshotCaptureSkippedForTesting;
    private ActivityTabProvider mActivityTabProvider;

    /**
     * Construct a new {@link ShareDelegateImpl}.
     * @param controller The BottomSheetController for the current activity.
     */
    public ShareDelegateImpl(BottomSheetController controller, ActivityTabProvider tabProvider,
            ShareSheetDelegate delegate) {
        mBottomSheetController = controller;
        mDelegate = delegate;
        mActivityTabProvider = tabProvider;
    }

    // ShareDelegate implementation.
    @Override
    public void share(ShareParams params) {
        mDelegate.share(params, mBottomSheetController, mActivityTabProvider);
    }

    // ShareDelegate implementation.
    @Override
    public void share(Tab currentTab, boolean shareDirectly) {
        onShareSelected(currentTab.getWindowAndroid().getActivity().get(), currentTab,
                shareDirectly, currentTab.isIncognito());
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    private void onShareSelected(
            Activity activity, Tab currentTab, boolean shareDirectly, boolean isIncognito) {
        if (currentTab == null) return;

        List<Class<? extends ShareActivity>> classesToEnable = new ArrayList<>(2);

        if (PrintShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(PrintShareActivity.class);
        }

        if (SendTabToSelfShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(SendTabToSelfShareActivity.class);
        }

        if (QrCodeShareActivity.featureIsAvailable()) {
            classesToEnable.add(QrCodeShareActivity.class);
        }

        if (!classesToEnable.isEmpty()) {
            OptionalShareTargetsManager.getInstance().enableOptionalShareActivities(activity,
                    classesToEnable, () -> triggerShare(currentTab, shareDirectly, isIncognito));
            return;
        }

        triggerShare(currentTab, shareDirectly, isIncognito);
    }

    protected void triggerShare(
            final Tab currentTab, final boolean shareDirectly, boolean isIncognito) {
        ScreenshotTabObserver tabObserver = ScreenshotTabObserver.from(currentTab);
        if (tabObserver != null) {
            tabObserver.onActionPerformedAfterScreenshot(
                    ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        }

        OfflinePageUtils.maybeShareOfflinePage(currentTab, (ShareParams p) -> {
            if (p != null) {
                mDelegate.share(p, mBottomSheetController, mActivityTabProvider);
            } else {
                WindowAndroid window = currentTab.getWindowAndroid();
                // Could not share as an offline page.
                if (shouldFetchCanonicalUrl(currentTab)) {
                    WebContents webContents = currentTab.getWebContents();
                    String title = currentTab.getTitle();
                    String visibleUrl = currentTab.getUrl();
                    webContents.getMainFrame().getCanonicalUrlForSharing(new Callback<String>() {
                        @Override
                        public void onResult(String result) {
                            logCanonicalUrlResult(visibleUrl, result);

                            triggerShareWithCanonicalUrlResolved(window, webContents, title,
                                    visibleUrl, result, shareDirectly, isIncognito);
                        }
                    });
                } else {
                    triggerShareWithCanonicalUrlResolved(window, currentTab.getWebContents(),
                            currentTab.getTitle(), currentTab.getUrl(), null, shareDirectly,
                            isIncognito);
                }
            }
        });
    }

    private void triggerShareWithCanonicalUrlResolved(final WindowAndroid window,
            final WebContents webContents, final String title, final String visibleUrl,
            final String canonicalUrl, final boolean shareDirectly, boolean isIncognito) {
        // Share an empty blockingUri in place of screenshot file. The file ready notification is
        // sent by onScreenshotReady call below when the file is written.
        final Uri blockingUri = (isIncognito || webContents == null)
                ? null
                : ChromeFileProvider.generateUriAndBlockAccess();
        ShareParams.Builder builder =
                new ShareParams.Builder(window, title, getUrlToShare(visibleUrl, canonicalUrl))
                        .setShareDirectly(shareDirectly)
                        .setSaveLastUsed(!shareDirectly)
                        .setScreenshotUri(blockingUri);
        mDelegate.share(builder.build(), mBottomSheetController, mActivityTabProvider);
        if (shareDirectly) {
            RecordUserAction.record("MobileMenuDirectShare");
        } else {
            RecordUserAction.record("MobileMenuShare");
        }

        if (blockingUri == null) return;

        // Start screenshot capture and notify the provider when it is ready.
        Callback<Uri> callback = (saveFile) -> {
            // Unblock the file once it is saved to disk.
            ChromeFileProvider.notifyFileReady(blockingUri, saveFile);
        };
        if (sScreenshotCaptureSkippedForTesting) {
            callback.onResult(null);
        } else {
            ShareHelper.captureScreenshotForContents(webContents, 0, 0, callback);
        }
    }

    @VisibleForTesting
    static boolean shouldFetchCanonicalUrl(final Tab currentTab) {
        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) return false;
        if (webContents.getMainFrame() == null) return false;
        String url = currentTab.getUrl();
        if (TextUtils.isEmpty(url)) return false;
        if (currentTab.isShowingErrorPage() || ((TabImpl) currentTab).isShowingInterstitialPage()
                || SadTab.isShowing(currentTab)) {
            return false;
        }
        return true;
    }

    private static void logCanonicalUrlResult(String visibleUrl, String canonicalUrl) {
        @CanonicalURLResult
        int result = getCanonicalUrlResult(visibleUrl, canonicalUrl);
        RecordHistogram.recordEnumeratedHistogram(CANONICAL_URL_RESULT_HISTOGRAM, result,
                CanonicalURLResult.CANONICAL_URL_RESULT_COUNT);
    }

    @VisibleForTesting
    public static void setScreenshotCaptureSkippedForTesting(boolean value) {
        sScreenshotCaptureSkippedForTesting = value;
    }

    @VisibleForTesting
    static String getUrlToShare(String visibleUrl, String canonicalUrl) {
        if (TextUtils.isEmpty(canonicalUrl)) return visibleUrl;
        // TODO(tedchoc): Can we replace GURLUtils.getScheme with Uri.parse(...).getScheme()
        //                https://crbug.com/783819
        if (!UrlConstants.HTTPS_SCHEME.equals(GURLUtils.getScheme(visibleUrl))) {
            return visibleUrl;
        }
        String canonicalScheme = GURLUtils.getScheme(canonicalUrl);
        if (!UrlConstants.HTTP_SCHEME.equals(canonicalScheme)
                && !UrlConstants.HTTPS_SCHEME.equals(canonicalScheme)) {
            return visibleUrl;
        }
        return canonicalUrl;
    }

    @CanonicalURLResult
    private static int getCanonicalUrlResult(String visibleUrl, String canonicalUrl) {
        if (!UrlConstants.HTTPS_SCHEME.equals(GURLUtils.getScheme(visibleUrl))) {
            return CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS;
        }
        if (TextUtils.isEmpty(canonicalUrl)) {
            return CanonicalURLResult.FAILED_NO_CANONICAL_URL_DEFINED;
        }
        String canonicalScheme = GURLUtils.getScheme(canonicalUrl);
        if (!UrlConstants.HTTPS_SCHEME.equals(canonicalScheme)) {
            if (!UrlConstants.HTTP_SCHEME.equals(canonicalScheme)) {
                return CanonicalURLResult.FAILED_CANONICAL_URL_INVALID;
            } else {
                return CanonicalURLResult.SUCCESS_CANONICAL_URL_NOT_HTTPS;
            }
        }
        if (TextUtils.equals(visibleUrl, canonicalUrl)) {
            return CanonicalURLResult.SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE;
        } else {
            return CanonicalURLResult.SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE;
        }
    }

    /**
     * Delegate for share handling.
     */
    public static class ShareSheetDelegate {
        /**
         * Trigger the share action for the specified params.
         */
        void share(ShareParams params, BottomSheetController controller,
                ActivityTabProvider tabProvider) {
            if (params.shareDirectly()) {
                ShareHelper.shareDirectly(params);
            } else if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB)) {
                ShareSheetCoordinator coordinator =
                        new ShareSheetCoordinator(controller, tabProvider);
                // TODO(crbug/1009124): open custom share sheet.
                coordinator.showShareSheet(params);
            } else if (ShareHelper.TargetChosenReceiver.isSupported()) {
                // On L+ open system share sheet.
                ShareHelper.makeIntentAndShare(params, null);
            } else {
                // On K and below open custom share dialog.
                ShareHelper.showShareDialog(params);
            }
        }
    }
}
