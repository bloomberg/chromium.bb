// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.support.annotation.IntDef;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.text.style.TextAppearanceSpan;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogView.ButtonType;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.page_info.PageInfoView.ConnectionInfoParams;
import org.chromium.chrome.browser.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.chrome.browser.page_info.PageInfoView.PermissionParams;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.ContentSettingsResources;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.vr_shell.UiUnsupportedMode;
import org.chromium.chrome.browser.vr_shell.VrShellDelegate;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.net.URISyntaxException;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Java side of Android implementation of the page info UI.
 */
public class PageInfoPopup implements ModalDialogView.Controller {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({OPENED_FROM_MENU, OPENED_FROM_TOOLBAR, OPENED_FROM_VR})
    private @interface OpenedFromSource {}

    public static final int OPENED_FROM_MENU = 1;
    public static final int OPENED_FROM_TOOLBAR = 2;
    public static final int OPENED_FROM_VR = 3;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({NOT_OFFLINE_PAGE, TRUSTED_OFFLINE_PAGE, UNTRUSTED_OFFLINE_PAGE})
    private @interface OfflinePageState {}

    public static final int NOT_OFFLINE_PAGE = 1;
    public static final int TRUSTED_OFFLINE_PAGE = 2;
    public static final int UNTRUSTED_OFFLINE_PAGE = 3;

    /**
     * An entry in the settings dropdown for a given permission. There are two options for each
     * permission: Allow and Block.
     */
    private static final class PageInfoPermissionEntry {
        public final String name;
        public final int type;
        public final ContentSetting setting;

        PageInfoPermissionEntry(String name, int type, ContentSetting setting) {
            this.name = name;
            this.type = type;
            this.setting = setting;
        }

        @Override
        public String toString() {
            return name;
        }
    }

    private static final int ENTER_START_DELAY_MS = 100;
    private static final int ENTER_EXIT_DURATION_MS = 200;
    private static final int CLOSE_CLEANUP_DELAY_MS = 10;

    private static final int MAX_MODAL_DIALOG_WIDTH_DP = 400;

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final Tab mTab;

    // A pointer to the C++ object for this UI.
    private long mNativePageInfoPopup;

    // The view inside the popup.
    private PageInfoView mView;

    // The dialog the container is placed in.
    // mSheetDialog is set if the dialog appears as a sheet. Otherwise, mModalDialog is set.
    private Dialog mSheetDialog;
    private ModalDialogView mModalDialog;

    // Whether or not the popup should appear at the bottom of the screen.
    private final boolean mIsBottomPopup;

    // Animation which is currently running, if there is one.
    private Animator mCurrentAnimation;

    private boolean mDismissWithoutAnimation;

    // The full URL from the URL bar, which is copied to the user's clipboard when they select 'Copy
    // URL'.
    private String mFullUrl;

    // A parsed version of mFullUrl. Is null if the URL is invalid/cannot be
    // parsed.
    private URI mParsedUrl;

    // Whether or not this page is an internal chrome page (e.g. the
    // chrome://settings page).
    private boolean mIsInternalPage;

    // The security level of the page (a valid ConnectionSecurityLevel).
    private int mSecurityLevel;

    // Permissions available to be displayed.
    private List<PageInfoPermissionEntry> mDisplayedPermissions;

    // Creation date of an offline copy, if web contents contains an offline page.
    private String mOfflinePageCreationDate;

    // The state of offline page in the web contents (not offline page, trusted/untrusted offline
    // page).
    private @OfflinePageState int mOfflinePageState;

    // The name of the content publisher, if any.
    private String mContentPublisher;

    // Observer for dismissing dialog if web contents get destroyed, navigate etc.
    private WebContentsObserver mWebContentsObserver;

    // A task that should be run once the page info popup is animated out and dismissed. Null if no
    // task is pending.
    private Runnable mPendingRunAfterDismissTask;

    /**
     * Creates the PageInfoPopup, but does not display it. Also initializes the corresponding
     * C++ object and saves a pointer to it.
     * @param activity                 Activity which is used for showing a popup.
     * @param tab                      Tab for which the pop up is shown.
     * @param offlinePageUrl           URL that the offline page claims to be generated from.
     * @param offlinePageCreationDate  Date when the offline page was created.
     * @param offlinePageState         State of the tab showing offline page.
     * @param publisher                The name of the content publisher, if any.
     */
    private PageInfoPopup(Activity activity, Tab tab, String offlinePageUrl,
            String offlinePageCreationDate, @OfflinePageState int offlinePageState,
            String publisher) {
        mContext = activity;
        mTab = tab;
        mIsBottomPopup = mTab.getActivity().getBottomSheet() != null;
        mOfflinePageState = offlinePageState;
        PageInfoViewParams viewParams = new PageInfoViewParams();

        if (mOfflinePageState != NOT_OFFLINE_PAGE) {
            mOfflinePageCreationDate = offlinePageCreationDate;
        }
        mWindowAndroid = mTab.getWebContents().getTopLevelNativeWindow();
        mContentPublisher = publisher;

        viewParams.alwaysShowFullUrl = mIsBottomPopup;
        viewParams.urlTitleClickCallback = () -> {
            // Expand/collapse the displayed URL title.
            mView.toggleUrlTruncation();
        };
        // Long press the url text to copy it to the clipboard.
        viewParams.urlTitleLongClickCallback = () -> {
            ClipboardManager clipboard =
                    (ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clip = ClipData.newPlainText("url", mFullUrl);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(mContext, R.string.url_copied, Toast.LENGTH_SHORT).show();
        };

        mDisplayedPermissions = new ArrayList<PageInfoPermissionEntry>();

        // Work out the URL and connection message and status visibility.
        mFullUrl = isShowingOfflinePage() ? offlinePageUrl : mTab.getOriginalUrl();

        // This can happen if an invalid chrome-distiller:// url was entered.
        if (mFullUrl == null) mFullUrl = "";

        try {
            mParsedUrl = new URI(mFullUrl);
            mIsInternalPage = UrlUtilities.isInternalScheme(mParsedUrl);
        } catch (URISyntaxException e) {
            mParsedUrl = null;
            mIsInternalPage = false;
        }
        mSecurityLevel = SecurityStateModel.getSecurityLevelForWebContents(mTab.getWebContents());

        String displayUrl = mFullUrl;
        if (isShowingOfflinePage()) {
            displayUrl = OfflinePageUtils.stripSchemeFromOnlineUrl(mFullUrl);
        }
        SpannableStringBuilder displayUrlBuilder = new SpannableStringBuilder(displayUrl);
        OmniboxUrlEmphasizer.emphasizeUrl(displayUrlBuilder, mContext.getResources(),
                mTab.getProfile(), mSecurityLevel, mIsInternalPage, true, true);
        if (mSecurityLevel == ConnectionSecurityLevel.SECURE) {
            OmniboxUrlEmphasizer.EmphasizeComponentsResponse emphasizeResponse =
                    OmniboxUrlEmphasizer.parseForEmphasizeComponents(
                            mTab.getProfile(), displayUrlBuilder.toString());
            if (emphasizeResponse.schemeLength > 0) {
                displayUrlBuilder.setSpan(
                        new TextAppearanceSpan(mContext, R.style.RobotoMediumStyle), 0,
                        emphasizeResponse.schemeLength, Spannable.SPAN_EXCLUSIVE_INCLUSIVE);
            }
        }
        viewParams.url = displayUrlBuilder;
        viewParams.urlOriginLength = OmniboxUrlEmphasizer.getOriginEndIndex(
                displayUrlBuilder.toString(), mTab.getProfile());

        if (mParsedUrl == null || mParsedUrl.getScheme() == null || isShowingOfflinePage()
                || !(mParsedUrl.getScheme().equals(UrlConstants.HTTP_SCHEME)
                           || mParsedUrl.getScheme().equals(UrlConstants.HTTPS_SCHEME))) {
            viewParams.siteSettingsButtonShown = false;
        } else {
            viewParams.siteSettingsButtonClickCallback = () -> {
                // Delay while the PageInfoPopup closes.
                runAfterDismiss(() -> {
                    recordAction(PageInfoAction.PAGE_INFO_SITE_SETTINGS_OPENED);
                    Bundle fragmentArguments =
                            SingleWebsitePreferences.createFragmentArgsForSite(mFullUrl);
                    Intent preferencesIntent = PreferencesLauncher.createIntentForSettingsPage(
                            mContext, SingleWebsitePreferences.class.getName());
                    preferencesIntent.putExtra(
                            Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArguments);
                    // Disabling StrictMode to avoid violations (https://crbug.com/819410).
                    try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                        mContext.startActivity(preferencesIntent);
                    }
                });
            };
        }

        if (isShowingOfflinePage()) {
            boolean isConnected = OfflinePageUtils.isConnected();
            RecordHistogram.recordBooleanHistogram(
                    "OfflinePages.WebsiteSettings.OpenOnlineButtonVisible", isConnected);
            if (isConnected) {
                viewParams.openOnlineButtonClickCallback = () -> {
                    runAfterDismiss(() -> {
                        // Attempt to reload to an online version of the viewed offline web page.
                        // This attempt might fail if the user is offline, in which case an offline
                        // copy will be reloaded.
                        RecordHistogram.recordBooleanHistogram(
                                "OfflinePages.WebsiteSettings.ConnectedWhenOpenOnlineButtonClicked",
                                OfflinePageUtils.isConnected());
                        OfflinePageUtils.reload(mTab);
                    });
                };
            } else {
                viewParams.openOnlineButtonShown = false;
            }
        } else {
            viewParams.openOnlineButtonShown = false;
        }

        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
        if (!mIsInternalPage && !isShowingOfflinePage()
                && instantAppsHandler.isInstantAppAvailable(mFullUrl, false /* checkHoldback */,
                           false /* includeUserPrefersBrowser */)) {
            final Intent instantAppIntent = instantAppsHandler.getInstantAppIntentForUrl(mFullUrl);
            viewParams.instantAppButtonClickCallback = () -> {
                try {
                    mContext.startActivity(instantAppIntent);
                    RecordUserAction.record("Android.InstantApps.LaunchedFromWebsiteSettingsPopup");
                } catch (ActivityNotFoundException e) {
                    mView.disableInstantAppButton();
                }
            };
            RecordUserAction.record("Android.InstantApps.OpenInstantAppButtonShown");
        } else {
            viewParams.instantAppButtonShown = false;
        }

        // On smaller screens, place the dialog at the top of the screen, and remove its border.
        if (isSheet()) {
            createSheetDialog();
        }

        mView = new PageInfoView(mContext, viewParams);
        mView.setVisibility(View.INVISIBLE);
        mView.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(
                    View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) {
                // Trigger the entrance animations once the main container has been laid out and has
                // a height.
                mView.removeOnLayoutChangeListener(this);
                mView.setVisibility(View.VISIBLE);
                createAllAnimations(true, null).start();
            }
        });

        // This needs to come after other member initialization.
        mNativePageInfoPopup = nativeInit(this, mTab.getWebContents());
        mWebContentsObserver = new WebContentsObserver(mTab.getWebContents()) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're showing
                // is stale so dismiss the dialog.
                dismissDialog();
            }

            @Override
            public void wasHidden() {
                // The web contents were hidden (potentially by loading another URL via an intent),
                // so dismiss the dialog).
                dismissDialog();
            }

            @Override
            public void destroy() {
                super.destroy();
                // Force the dialog to close immediately in case the destroy was from Chrome
                // quitting.
                mDismissWithoutAnimation = true;
                dismissDialog();
            }
        };

        showDialog();
    }

    /**
     * Finds the Image resource of the icon to use for the given permission.
     *
     * @param permission A valid ContentSettingsType that can be displayed in the PageInfo dialog to
     *                   retrieve the image for.
     * @return The resource ID of the icon to use for that permission.
     */
    private int getImageResourceForPermission(int permission) {
        int icon = ContentSettingsResources.getIcon(permission);
        assert icon != 0 : "Icon requested for invalid permission: " + permission;
        return icon;
    }

    /**
     * Whether to show a 'Details' link to the connection info popup. The link is only shown for
     * HTTPS connections.
     */
    private boolean isConnectionDetailsLinkVisible() {
        return mContentPublisher == null && !isShowingOfflinePage() && mParsedUrl != null
                && mParsedUrl.getScheme() != null
                        && mParsedUrl.getScheme().equals(UrlConstants.HTTPS_SCHEME);
    }

    private boolean hasAndroidPermission(int contentSettingType) {
        String[] androidPermissions =
                PrefServiceBridge.getAndroidPermissionsForContentSetting(contentSettingType);
        if (androidPermissions == null) return true;
        for (int i = 0; i < androidPermissions.length; i++) {
            if (!mWindowAndroid.hasPermission(androidPermissions[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param type The ContentSettingsType of the permission.
     * @param currentSettingValue The ContentSetting value of the currently selected setting.
     */
    @CalledByNative
    private void addPermissionSection(String name, int type, int currentSettingValue) {
        mDisplayedPermissions.add(new PageInfoPermissionEntry(name, type, ContentSetting
                .fromInt(currentSettingValue)));
    }

    /**
     * Update the permissions view based on the contents of mDisplayedPermissions.
     */
    @CalledByNative
    private void updatePermissionDisplay() {
        List<PermissionParams> permissionParamsList = new ArrayList<>();
        for (PageInfoPermissionEntry permission : mDisplayedPermissions) {
            permissionParamsList.add(createPermissionParams(permission));
        }
        mView.setPermissions(permissionParamsList);
    }

    private Runnable createPermissionClickCallback(
            Intent intentOverride, String[] androidPermissions) {
        return () -> {
            if (intentOverride == null && mWindowAndroid != null) {
                // Try and immediately request missing Android permissions where possible.
                for (int i = 0; i < androidPermissions.length; i++) {
                    if (!mWindowAndroid.canRequestPermission(androidPermissions[i])) continue;

                    // If any permissions can be requested, attempt to request them all.
                    mWindowAndroid.requestPermissions(androidPermissions, new PermissionCallback() {
                        @Override
                        public void onRequestPermissionsResult(
                                String[] permissions, int[] grantResults) {
                            boolean allGranted = true;
                            for (int i = 0; i < grantResults.length; i++) {
                                if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                                    allGranted = false;
                                    break;
                                }
                            }
                            if (allGranted) updatePermissionDisplay();
                        }
                    });
                    return;
                }
            }

            runAfterDismiss(() -> {
                Intent settingsIntent;
                if (intentOverride != null) {
                    settingsIntent = intentOverride;
                } else {
                    settingsIntent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                    settingsIntent.setData(Uri.parse("package:" + mContext.getPackageName()));
                }
                settingsIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mContext.startActivity(settingsIntent);
            });
        };
    }

    private PermissionParams createPermissionParams(PageInfoPermissionEntry permission) {
        PermissionParams permissionParams = new PermissionParams();

        permissionParams.iconResource = getImageResourceForPermission(permission.type);
        if (permission.setting == ContentSetting.ALLOW) {
            LocationUtils locationUtils = LocationUtils.getInstance();
            Intent intentOverride = null;
            String[] androidPermissions = null;
            if (permission.type == ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION
                    && !locationUtils.isSystemLocationSettingEnabled()) {
                permissionParams.warningTextResource = R.string.page_info_android_location_blocked;
                intentOverride = locationUtils.getSystemLocationSettingsIntent();
            } else if (!hasAndroidPermission(permission.type)) {
                permissionParams.warningTextResource =
                        R.string.page_info_android_permission_blocked;
                androidPermissions =
                        PrefServiceBridge.getAndroidPermissionsForContentSetting(permission.type);
            }

            if (permissionParams.warningTextResource != 0) {
                permissionParams.iconResource = R.drawable.exclamation_triangle;
                permissionParams.iconTintColorResource = R.color.google_blue_700;
                permissionParams.clickCallback =
                        createPermissionClickCallback(intentOverride, androidPermissions);
            }
        }

        // The ads permission requires an additional static subtitle.
        if (permission.type == ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS) {
            permissionParams.subtitleTextResource = R.string.page_info_permission_ads_subtitle;
        }

        SpannableStringBuilder builder = new SpannableStringBuilder();
        SpannableString nameString = new SpannableString(permission.name);
        final StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        nameString.setSpan(boldSpan, 0, nameString.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);

        builder.append(nameString);
        builder.append(" – "); // en-dash.
        String status_text = "";
        switch (permission.setting) {
            case ALLOW:
                status_text = mContext.getString(R.string.page_info_permission_allowed);
                break;
            case BLOCK:
                status_text = mContext.getString(R.string.page_info_permission_blocked);
                break;
            default:
                assert false : "Invalid setting " + permission.setting + " for permission "
                               + permission.type;
        }
        if (WebsitePreferenceBridge.isPermissionControlledByDSE(permission.type, mFullUrl, false)) {
            status_text = statusTextForDSEPermission(permission.setting);
        }
        builder.append(status_text);
        permissionParams.status = builder;

        return permissionParams;
    }

    /**
     * Returns the permission string for the Default Search Engine.
     */
    private String statusTextForDSEPermission(ContentSetting setting) {
        if (setting == ContentSetting.ALLOW) {
            return mContext.getString(R.string.page_info_dse_permission_allowed);
        }

        return mContext.getString(R.string.page_info_dse_permission_blocked);
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    @CalledByNative
    private void setSecurityDescription(String summary, String details) {
        ConnectionInfoParams connectionInfoParams = new ConnectionInfoParams();

        // Display the appropriate connection message.
        SpannableStringBuilder messageBuilder = new SpannableStringBuilder();
        if (mContentPublisher != null) {
            messageBuilder.append(
                    mContext.getString(R.string.page_info_domain_hidden, mContentPublisher));
        } else if (mOfflinePageState == TRUSTED_OFFLINE_PAGE) {
            messageBuilder.append(
                    String.format(mContext.getString(R.string.page_info_connection_offline),
                            mOfflinePageCreationDate));
        } else if (mOfflinePageState == UNTRUSTED_OFFLINE_PAGE) {
            // For untrusted pages, if there's a creation date, show it in the message.
            if (TextUtils.isEmpty(mOfflinePageCreationDate)) {
                messageBuilder.append(mContext.getString(
                        R.string.page_info_offline_page_not_trusted_without_date));
            } else {
                messageBuilder.append(String.format(
                        mContext.getString(R.string.page_info_offline_page_not_trusted_with_date),
                        mOfflinePageCreationDate));
            }
        } else {
            if (!TextUtils.equals(summary, details)) {
                connectionInfoParams.summary = summary;
            }
            messageBuilder.append(details);
        }

        if (isConnectionDetailsLinkVisible()) {
            messageBuilder.append(" ");
            SpannableString detailsText =
                    new SpannableString(mContext.getString(R.string.details_link));
            final ForegroundColorSpan blueSpan =
                    new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                            mContext.getResources(), R.color.google_blue_700));
            detailsText.setSpan(
                    blueSpan, 0, detailsText.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            messageBuilder.append(detailsText);
        }

        connectionInfoParams.message = messageBuilder;
        if (isConnectionDetailsLinkVisible()) {
            connectionInfoParams.clickCallback = () -> {
                runAfterDismiss(() -> {
                    // TODO(crbug.com/819883): Port the connection info popup to VR.
                    if (VrShellDelegate.isInVr()) {
                        VrShellDelegate.requestToExitVrAndRunOnSuccess(
                                PageInfoPopup.this ::showConnectionInfoPopup,
                                UiUnsupportedMode.UNHANDLED_CONNECTION_INFO);
                    } else {
                        showConnectionInfoPopup();
                    }
                });
            };
        }
        mView.setConnectionInfo(connectionInfoParams);
    }

    /**
     * Displays the PageInfoPopup.
     */
    private void showDialog() {
        ScrollView dialogView;
        if (isSheet()) {
            // On smaller screens, make the dialog fill the width of the screen.
            dialogView = new ScrollView(mContext) {
                @Override
                protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                    int dialogMaxHeight = mTab.getHeight();
                    if (mIsBottomPopup) {
                        // In Chrome Home, the full URL is showing at all times; give the scroll
                        // view a max height so long URLs don't consume the entire screen.
                        dialogMaxHeight = (int) (dialogMaxHeight
                                - mContext.getResources().getDimension(
                                          R.dimen.min_touch_target_size));
                    }

                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(dialogMaxHeight, MeasureSpec.AT_MOST);
                    super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                }
            };
        } else {
            // On larger screens, make the dialog centered in the screen and have a maximum width.
            dialogView = new ScrollView(mContext) {
                @Override
                protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                    final int maxDialogWidthInPx = (int) (MAX_MODAL_DIALOG_WIDTH_DP
                            * mContext.getResources().getDisplayMetrics().density);
                    if (MeasureSpec.getSize(widthMeasureSpec) > maxDialogWidthInPx) {
                        widthMeasureSpec = MeasureSpec.makeMeasureSpec(maxDialogWidthInPx,
                                MeasureSpec.EXACTLY);
                    }
                    super.onMeasure(widthMeasureSpec, heightMeasureSpec);
                }
            };
        }

        dialogView.addView(mView);

        if (isSheet()) {
            mSheetDialog.addContentView(dialogView,
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.MATCH_PARENT));

            // This must be called after addContentView, or it won't fully fill to the edge.
            Window window = mSheetDialog.getWindow();
            window.setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mSheetDialog.show();
        } else {
            ModalDialogView.Params params = new ModalDialogView.Params();
            params.customView = dialogView;
            params.cancelOnTouchOutside = true;
            mModalDialog = new ModalDialogView(this, params);
            mTab.getActivity().getModalDialogManager().showDialog(
                    mModalDialog, ModalDialogManager.APP_MODAL);
        }
    }

    /**
     * Dismiss the popup, and then run a task after the animation has completed (if there is one).
     */
    private void runAfterDismiss(Runnable task) {
        mPendingRunAfterDismissTask = task;
        dismissDialog();
    }

    @Override
    public void onClick(@ButtonType int buttonType) {}

    @Override
    public void onCancel() {}

    @Override
    public void onDismiss() {
        assert mNativePageInfoPopup != 0;
        mWebContentsObserver.destroy();
        nativeDestroy(mNativePageInfoPopup);
        mNativePageInfoPopup = 0;
        if (mPendingRunAfterDismissTask != null) {
            mPendingRunAfterDismissTask.run();
            mPendingRunAfterDismissTask = null;
        }
    }

    private void dismissDialog() {
        if (isSheet()) {
            mSheetDialog.dismiss();
        } else {
            mTab.getActivity().getModalDialogManager().dismissDialog(mModalDialog);
        }
    }

    /**
     * Create an animator to slide in the entire dialog from the top of the screen.
     */
    private Animator createDialogSlideAnimaton(boolean isEnter) {
        final float animHeight = (mIsBottomPopup ? 1f : -1f) * mView.getHeight();
        ObjectAnimator translateAnim;
        if (isEnter) {
            mView.setTranslationY(animHeight);
            translateAnim = ObjectAnimator.ofFloat(mView, View.TRANSLATION_Y, 0f);
            translateAnim.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        } else {
            translateAnim = ObjectAnimator.ofFloat(mView, View.TRANSLATION_Y, animHeight);
            translateAnim.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        }
        translateAnim.setDuration(ENTER_EXIT_DURATION_MS);
        return translateAnim;
    }

    /**
     * Create an animator to show/hide the entire dialog. On phones the dialog is slid in as a
     * sheet. Otherwise, the default fade-in is used.
     */
    private Animator createAllAnimations(boolean isEnter, Runnable onAnimationEnd) {
        Animator dialogAnimation =
                isSheet() ? createDialogSlideAnimaton(isEnter) : new AnimatorSet();
        Animator viewAnimation =
                isEnter ? mView.createEnterAnimation() : mView.createExitAnimation();
        AnimatorSet allAnimations = new AnimatorSet();
        if (isEnter) allAnimations.setStartDelay(ENTER_START_DELAY_MS);
        allAnimations.playTogether(dialogAnimation, viewAnimation);
        allAnimations.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentAnimation = null;
                if (onAnimationEnd == null) return;
                onAnimationEnd.run();
            }
        });
        if (mCurrentAnimation != null) mCurrentAnimation.cancel();
        mCurrentAnimation = allAnimations;
        return allAnimations;
    }

    private void recordAction(int action) {
        if (mNativePageInfoPopup != 0) {
            nativeRecordPageInfoAction(mNativePageInfoPopup, action);
        }
    }

    /**
     * Whether website dialog is displayed for an offline page.
     */
    private boolean isShowingOfflinePage() {
        return mOfflinePageState != NOT_OFFLINE_PAGE;
    }

    private boolean isSheet() {
        return !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                && !VrShellDelegate.isInVr();
    }

    private void createSheetDialog() {
        mSheetDialog = new Dialog(mContext) {
            private void superDismiss() {
                super.dismiss();
            }

            @Override
            public void dismiss() {
                if (mDismissWithoutAnimation) {
                    // Dismiss the modal dialogs without any custom animations.
                    super.dismiss();
                } else {
                    createAllAnimations(false, () -> {
                        // onAnimationEnd is called during the final frame of the animation.
                        // Delay the cleanup by a tiny amount to give this frame a chance to
                        // be displayed before we destroy the dialog.
                        mView.postDelayed(this ::superDismiss, CLOSE_CLEANUP_DELAY_MS);
                    }).start();
                }
            }
        };
        mSheetDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mSheetDialog.setCanceledOnTouchOutside(true);

        Window window = mSheetDialog.getWindow();
        window.setGravity(Gravity.TOP);
        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        mSheetDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                PageInfoPopup.this.onDismiss();
            }
        });

        if (mIsBottomPopup) {
            mSheetDialog.getWindow().getAttributes().gravity = Gravity.BOTTOM | Gravity.END;
        }
    }

    private void showConnectionInfoPopup() {
        if (!mTab.getWebContents().isDestroyed()) {
            recordAction(PageInfoAction.PAGE_INFO_SECURITY_DETAILS_OPENED);
            ConnectionInfoPopup.show(mContext, mTab.getWebContents());
        }
    }

    /**
     * Shows a PageInfo dialog for the provided Tab. The popup adds itself to the view
     * hierarchy which owns the reference while it's visible.
     *
     * @param activity Activity which is used for launching a dialog.
     * @param tab The tab hosting the web contents for which to show Website information. This
     *            information is retrieved for the visible entry.
     * @param contentPublisher The name of the publisher of the content.
     * @param source Determines the source that triggered the popup.
     */
    public static void show(final Activity activity, final Tab tab, final String contentPublisher,
            @OpenedFromSource int source) {
        if (source == OPENED_FROM_MENU) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromMenu");
        } else if (source == OPENED_FROM_TOOLBAR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromToolbar");
        } else if (source == OPENED_FROM_VR) {
            RecordUserAction.record("MobileWebsiteSettingsOpenedFromVR");
        } else {
            assert false : "Invalid source passed";
        }

        String offlinePageUrl = null;
        String offlinePageCreationDate = null;
        @OfflinePageState
        int offlinePageState = NOT_OFFLINE_PAGE;

        OfflinePageItem offlinePage = OfflinePageUtils.getOfflinePage(tab);
        if (offlinePage != null) {
            offlinePageUrl = offlinePage.getUrl();
            if (OfflinePageUtils.isShowingTrustedOfflinePage(tab)) {
                offlinePageState = TRUSTED_OFFLINE_PAGE;
            } else {
                offlinePageState = UNTRUSTED_OFFLINE_PAGE;
            }
            // Get formatted creation date of the offline page. If the page was shared (so the
            // creation date cannot be acquired), make date an empty string and there will be
            // specific processing for showing different string in UI.
            long pageCreationTimeMs = offlinePage.getCreationTimeMs();
            if (pageCreationTimeMs != 0) {
                Date creationDate = new Date(offlinePage.getCreationTimeMs());
                DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
                offlinePageCreationDate = df.format(creationDate);
            }
        }

        new PageInfoPopup(activity, tab, offlinePageUrl, offlinePageCreationDate, offlinePageState,
                contentPublisher);
    }

    private static native long nativeInit(PageInfoPopup popup, WebContents webContents);

    private native void nativeDestroy(long nativePageInfoPopupAndroid);

    private native void nativeRecordPageInfoAction(
            long nativePageInfoPopupAndroid, int action);
}
