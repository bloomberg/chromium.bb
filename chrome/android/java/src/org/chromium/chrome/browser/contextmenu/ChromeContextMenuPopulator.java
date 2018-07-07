// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.net.MailTo;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
import android.text.TextUtils;
import android.util.Pair;
import android.view.ContextMenu;
import android.webkit.MimeTypeMap;

import org.chromium.base.CollectionUtil;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.common.ContentUrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A {@link ContextMenuPopulator} used for showing the default Chrome context menu.
 */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private static final String TAG = "CCMenuPopulator";
    private static final ShareContextMenuItem SHARE_IMAGE =
            new ShareContextMenuItem(R.drawable.ic_share_white_24dp,
                    R.string.contextmenu_share_image, R.id.contextmenu_share_image, false);
    private static final ShareContextMenuItem SHARE_LINK =
            new ShareContextMenuItem(R.drawable.ic_share_white_24dp,
                    R.string.contextmenu_share_link, R.id.contextmenu_share_link, true);

    /**
     * Defines the Groups of each Context Menu Item
     */
    @IntDef({ContextMenuGroup.LINK, ContextMenuGroup.IMAGE, ContextMenuGroup.VIDEO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuGroup {
        int LINK = 0;
        int IMAGE = 1;
        int VIDEO = 2;
    }

    /**
     * Defines the context menu modes
     */
    @IntDef({ContextMenuMode.NORMAL, ContextMenuMode.CUSTOM_TAB, ContextMenuMode.WEB_APP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContextMenuMode {
        int NORMAL = 0; /* Default mode*/
        int CUSTOM_TAB = 1; /* Custom tab mode */
        int WEB_APP = 2; /* Full screen mode */
    }

    // Items that are included in all context menus.
    private static final Set<? extends ContextMenuItem> BASE_WHITELIST =
            Collections.unmodifiableSet(CollectionUtil.newHashSet(
                    ChromeContextMenuItem.COPY_LINK_ADDRESS, ChromeContextMenuItem.CALL,
                    ChromeContextMenuItem.SEND_MESSAGE, ChromeContextMenuItem.ADD_TO_CONTACTS,
                    ChromeContextMenuItem.COPY, ChromeContextMenuItem.COPY_LINK_TEXT,
                    ChromeContextMenuItem.LOAD_ORIGINAL_IMAGE, ChromeContextMenuItem.SAVE_LINK_AS,
                    ChromeContextMenuItem.SAVE_IMAGE, SHARE_IMAGE, ChromeContextMenuItem.SAVE_VIDEO,
                    SHARE_LINK));

    // Items that are included for normal Chrome browser mode.
    private static final Set<? extends ContextMenuItem> NORMAL_MODE_WHITELIST =
            Collections.unmodifiableSet(CollectionUtil.newHashSet(
                    ChromeContextMenuItem.OPEN_IN_NEW_TAB,
                    ChromeContextMenuItem.OPEN_IN_OTHER_WINDOW,
                    ChromeContextMenuItem.OPEN_IN_INCOGNITO_TAB, ChromeContextMenuItem.SAVE_LINK_AS,
                    ChromeContextMenuItem.OPEN_IMAGE_IN_NEW_TAB,
                    ChromeContextMenuItem.SEARCH_BY_IMAGE));

    // Additional items for custom tabs mode.
    private static final Set<? extends ContextMenuItem> CUSTOM_TAB_MODE_WHITELIST =
            Collections.unmodifiableSet(CollectionUtil.newHashSet(ChromeContextMenuItem.OPEN_IMAGE,
                    ChromeContextMenuItem.SEARCH_BY_IMAGE,
                    ChromeContextMenuItem.OPEN_IN_NEW_CHROME_TAB,
                    ChromeContextMenuItem.OPEN_IN_CHROME_INCOGNITO_TAB,
                    ChromeContextMenuItem.OPEN_IN_BROWSER_ID));

    // Additional items for fullscreen tabs mode.
    private static final Set<? extends ContextMenuItem> WEB_APP_MODE_WHITELIST =
            Collections.unmodifiableSet(
                    CollectionUtil.newHashSet(ChromeContextMenuItem.OPEN_IN_CHROME));

    // The order of the items within each lists determines the order of the context menu.
    private static final List<? extends ContextMenuItem> CUSTOM_TAB_GROUP =
            Collections.unmodifiableList(
                    CollectionUtil.newArrayList(ChromeContextMenuItem.OPEN_IN_NEW_CHROME_TAB,
                            ChromeContextMenuItem.OPEN_IN_CHROME_INCOGNITO_TAB,
                            ChromeContextMenuItem.OPEN_IN_BROWSER_ID));

    private static final List<? extends ContextMenuItem> LINK_GROUP = Collections.unmodifiableList(
            CollectionUtil.newArrayList(ChromeContextMenuItem.OPEN_IN_NEW_TAB,
                    ChromeContextMenuItem.OPEN_IN_INCOGNITO_TAB,
                    ChromeContextMenuItem.OPEN_IN_OTHER_WINDOW,
                    ChromeContextMenuItem.COPY_LINK_ADDRESS, ChromeContextMenuItem.COPY_LINK_TEXT,
                    ChromeContextMenuItem.SAVE_LINK_AS, SHARE_LINK));

    private static final List<? extends ContextMenuItem> IMAGE_GROUP =
            Collections.unmodifiableList(CollectionUtil.newArrayList(
                    ChromeContextMenuItem.LOAD_ORIGINAL_IMAGE, ChromeContextMenuItem.OPEN_IMAGE,
                    ChromeContextMenuItem.OPEN_IMAGE_IN_NEW_TAB, ChromeContextMenuItem.SAVE_IMAGE,
                    ChromeContextMenuItem.SEARCH_BY_IMAGE, SHARE_IMAGE));

    private static final List<? extends ContextMenuItem> MESSAGE_GROUP =
            Collections.unmodifiableList(CollectionUtil.newArrayList(ChromeContextMenuItem.CALL,
                    ChromeContextMenuItem.SEND_MESSAGE, ChromeContextMenuItem.ADD_TO_CONTACTS,
                    ChromeContextMenuItem.COPY));

    private static final List<? extends ContextMenuItem> VIDEO_GROUP = Collections.unmodifiableList(
            CollectionUtil.newArrayList(ChromeContextMenuItem.SAVE_VIDEO));

    private static final List<? extends ContextMenuItem> OTHER_GROUP = Collections.unmodifiableList(
            CollectionUtil.newArrayList(ChromeContextMenuItem.OPEN_IN_CHROME));

    private final ContextMenuItemDelegate mDelegate;
    private final @ContextMenuMode int mMode;

    static class ContextMenuUma {
        // Note: these values must match the ContextMenuOption enum in histograms.xml.
        @IntDef({Action.OPEN_IN_NEW_TAB, Action.OPEN_IN_INCOGNITO_TAB, Action.COPY_LINK_ADDRESS,
                Action.COPY_EMAIL_ADDRESS, Action.COPY_LINK_TEXT, Action.SAVE_LINK,
                Action.SAVE_IMAGE, Action.OPEN_IMAGE, Action.OPEN_IMAGE_IN_NEW_TAB,
                Action.SEARCH_BY_IMAGE, Action.LOAD_ORIGINAL_IMAGE, Action.SAVE_VIDEO,
                Action.SHARE_IMAGE, Action.OPEN_IN_OTHER_WINDOW, Action.SEND_EMAIL,
                Action.ADD_TO_CONTACTS, Action.CALL, Action.SEND_TEXT_MESSAGE,
                Action.COPY_PHONE_NUMBER, Action.OPEN_IN_NEW_CHROME_TAB,
                Action.OPEN_IN_CHROME_INCOGNITO_TAB, Action.OPEN_IN_BROWSER, Action.OPEN_IN_CHROME,
                Action.SHARE_LINK})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Action {
            int OPEN_IN_NEW_TAB = 0;
            int OPEN_IN_INCOGNITO_TAB = 1;
            int COPY_LINK_ADDRESS = 2;
            int COPY_EMAIL_ADDRESS = 3;
            int COPY_LINK_TEXT = 4;
            int SAVE_LINK = 5;
            int SAVE_IMAGE = 6;
            int OPEN_IMAGE = 7;
            int OPEN_IMAGE_IN_NEW_TAB = 8;
            int SEARCH_BY_IMAGE = 11;
            int LOAD_ORIGINAL_IMAGE = 13;
            int SAVE_VIDEO = 14;
            int SHARE_IMAGE = 19;
            int OPEN_IN_OTHER_WINDOW = 20;
            int SEND_EMAIL = 23;
            int ADD_TO_CONTACTS = 24;
            int CALL = 30;
            int SEND_TEXT_MESSAGE = 31;
            int COPY_PHONE_NUMBER = 32;
            int OPEN_IN_NEW_CHROME_TAB = 33;
            int OPEN_IN_CHROME_INCOGNITO_TAB = 34;
            int OPEN_IN_BROWSER = 35;
            int OPEN_IN_CHROME = 36;
            int SHARE_LINK = 37;
            int NUM_ENTRIES = 38;
        }

        // Note: these values must match the ContextMenuSaveLinkType enum in histograms.xml.
        // Only add new values at the end, right before NUM_TYPES. We depend on these specific
        // values in UMA histograms.
        @IntDef({Type.UNKNOWN, Type.TEXT, Type.IMAGE, Type.AUDIO, Type.VIDEO, Type.PDF})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Type {
            int UNKNOWN = 0;
            int TEXT = 1;
            int IMAGE = 2;
            int AUDIO = 3;
            int VIDEO = 4;
            int PDF = 5;
            int NUM_ENTRIES = 6;
        }

        // Note: these values must match the ContextMenuSaveImage enum in histograms.xml.
        // Only add new values at the end, right before NUM_ENTRIES.
        @IntDef({TypeSaveImage.LOADED, TypeSaveImage.FETCHED_LOFI, TypeSaveImage.NOT_DOWNLOADABLE,
                TypeSaveImage.DISABLED_AND_IS_NOT_IMAGE_PARAM,
                TypeSaveImage.DISABLED_AND_IS_IMAGE_PARAM, TypeSaveImage.SHOWN})
        @Retention(RetentionPolicy.SOURCE)
        public @interface TypeSaveImage {
            int LOADED = 0;
            int FETCHED_LOFI = 1;
            int NOT_DOWNLOADABLE = 2;
            int DISABLED_AND_IS_NOT_IMAGE_PARAM = 3;
            int DISABLED_AND_IS_IMAGE_PARAM = 4;
            int SHOWN = 5;
            int NUM_ENTRIES = 6;
        }

        /**
         * Records a histogram entry when the user selects an item from a context menu.
         * @param params The ContextMenuParams describing the current context menu.
         * @param action The action that the user selected (e.g. ACTION_SAVE_IMAGE).
         */
        static void record(ContextMenuParams params, @Action int action) {
            String histogramName;
            if (params.isVideo()) {
                histogramName = "ContextMenu.SelectedOption.Video";
            } else if (params.isImage()) {
                histogramName = params.isAnchor()
                        ? "ContextMenu.SelectedOption.ImageLink"
                        : "ContextMenu.SelectedOption.Image";
            } else {
                assert params.isAnchor();
                histogramName = "ContextMenu.SelectedOption.Link";
            }
            RecordHistogram.recordEnumeratedHistogram(histogramName, action, Action.NUM_ENTRIES);
        }

        /**
         * Records the content types when user downloads the file by long pressing the
         * save link context menu option.
         */
        static void recordSaveLinkTypes(String url) {
            String extension = MimeTypeMap.getFileExtensionFromUrl(url);
            @Type
            int mimeType = Type.UNKNOWN;
            if (extension != null) {
                String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
                if (type != null) {
                    if (type.startsWith("text")) {
                        mimeType = Type.TEXT;
                    } else if (type.startsWith("image")) {
                        mimeType = Type.IMAGE;
                    } else if (type.startsWith("audio")) {
                        mimeType = Type.AUDIO;
                    } else if (type.startsWith("video")) {
                        mimeType = Type.VIDEO;
                    } else if (type.equals("application/pdf")) {
                        mimeType = Type.PDF;
                    }
                }
            }
            RecordHistogram.recordEnumeratedHistogram(
                    "ContextMenu.SaveLinkType", mimeType, Type.NUM_ENTRIES);
        }

        /**
         * Helper method to record MobileDownload.ContextMenu.SaveImage UMA
         * @param type Type to record
         */
        static void recordSaveImageUma(int type) {
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileDownload.ContextMenu.SaveImage", type, TypeSaveImage.NUM_ENTRIES);
        }
    }

    /**
     * Builds a {@link ChromeContextMenuPopulator}.
     * @param delegate The {@link ContextMenuItemDelegate} that will be notified with actions
     *                 to perform when menu items are selected.
     * @param mode Defines the context menu mode
     */
    public ChromeContextMenuPopulator(ContextMenuItemDelegate delegate, @ContextMenuMode int mode) {
        mDelegate = delegate;
        mMode = mode;
    }

    @Override
    public void onDestroy() {
        mDelegate.onDestroy();
    }

    /**
     * Gets the link of the item or the alternate text of an image.
     * @return A string with either the link or with the alternate text.
     */
    public static String createHeaderText(ContextMenuParams params) {
        if (!isEmptyUrl(params.getLinkUrl())) {
            // The context menu can be created without native library
            // being loaded. Only use native URL formatting methods
            // if the native libraries have been loaded.
            if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isStartupSuccessfullyCompleted()) {
                return UrlFormatter.formatUrlForDisplayOmitHTTPScheme(params.getLinkUrl());
            } else {
                return params.getLinkUrl();
            }
        } else if (!TextUtils.isEmpty(params.getTitleText())) {
            return params.getTitleText();
        }
        return "";
    }

    @Override
    public List<Pair<Integer, List<ContextMenuItem>>> buildContextMenu(
            ContextMenu menu, Context context, ContextMenuParams params) {
        // Add all items in a group
        Set<ContextMenuItem> supportedOptions = new HashSet<>();
        if (FirstRunStatus.getFirstRunFlowComplete()) {
            supportedOptions.addAll(BASE_WHITELIST);
            if (mMode == ContextMenuMode.WEB_APP) {
                supportedOptions.addAll(WEB_APP_MODE_WHITELIST);
            } else if (mMode == ContextMenuMode.CUSTOM_TAB) {
                supportedOptions.addAll(CUSTOM_TAB_MODE_WHITELIST);
            } else {
                supportedOptions.addAll(NORMAL_MODE_WHITELIST);
            }
        } else {
            supportedOptions.add(ChromeContextMenuItem.COPY_LINK_ADDRESS);
            supportedOptions.add(ChromeContextMenuItem.COPY_LINK_TEXT);
            supportedOptions.add(ChromeContextMenuItem.COPY);
        }

        Set<ContextMenuItem> disabledOptions = getDisabledOptions(params);
        // Split the items into their respective groups.
        List<Pair<Integer, List<ContextMenuItem>>> groupedItems = new ArrayList<>();
        if (params.isAnchor()) {
            populateItemGroup(ContextMenuGroup.LINK, R.string.contextmenu_link_title, groupedItems,
                    supportedOptions, disabledOptions);
        }
        if (params.isImage()) {
            populateItemGroup(ContextMenuGroup.IMAGE, R.string.contextmenu_image_title,
                    groupedItems, supportedOptions, disabledOptions);
        }
        if (params.isVideo()) {
            populateItemGroup(ContextMenuGroup.VIDEO, R.string.contextmenu_video_title,
                    groupedItems, supportedOptions, disabledOptions);
        }

        // If there are no groups there still needs to be a way to add items from the OTHER_GROUP
        // and CUSTOM_TAB_GROUP.
        if (groupedItems.isEmpty()) {
            int titleResId = R.string.contextmenu_link_title;

            if (params.isVideo()) {
                titleResId = R.string.contextmenu_video_title;
            } else if (params.isImage()) {
                titleResId = R.string.contextmenu_image_title;
            }
            groupedItems.add(new Pair<Integer, List<ContextMenuItem>>(
                    titleResId, new ArrayList<ContextMenuItem>()));
        }

        // These items don't belong to any official group so they are added to a possible visible
        // list.
        addValidItems(groupedItems.get(groupedItems.size() - 1).second, OTHER_GROUP,
                supportedOptions, disabledOptions);
        if (mMode == ContextMenuMode.CUSTOM_TAB) {
            addValidItemsToFront(groupedItems.get(0).second, CUSTOM_TAB_GROUP, supportedOptions,
                    disabledOptions);
        }

        // If there are no items from the extra items within OTHER_GROUP and CUSTOM_TAB_GROUP, then
        // it's removed since there is nothing to show at all.
        if (groupedItems.get(0).second.isEmpty()) {
            groupedItems.remove(0);
        }

        if (!groupedItems.isEmpty()) {
            boolean hasSaveImage = false;
            for (int i = 0; i < groupedItems.size(); ++i) {
                Pair<Integer, List<ContextMenuItem>> menuList = groupedItems.get(i);
                if (menuList.second != null
                        && menuList.second.contains(ChromeContextMenuItem.SAVE_IMAGE)) {
                    hasSaveImage = true;
                    break;
                }
            }

            if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isStartupSuccessfullyCompleted()) {
                if (!hasSaveImage) {
                    ContextMenuUma.recordSaveImageUma(params.isImage()
                                    ? ContextMenuUma.TypeSaveImage.DISABLED_AND_IS_IMAGE_PARAM
                                    : ContextMenuUma.TypeSaveImage.DISABLED_AND_IS_NOT_IMAGE_PARAM);
                } else {
                    ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.SHOWN);
                }
            }
        }

        return groupedItems;
    }

    private void populateItemGroup(@ContextMenuGroup int contextMenuType, @StringRes int titleResId,
            List<Pair<Integer, List<ContextMenuItem>>> itemGroups,
            Set<ContextMenuItem> supportedOptions, Set<ContextMenuItem> disabledOptions) {
        List<ContextMenuItem> items = new ArrayList<>();
        switch (contextMenuType) {
            case ContextMenuGroup.LINK:
                addValidItems(items, LINK_GROUP, supportedOptions, disabledOptions);
                addValidItems(items, MESSAGE_GROUP, supportedOptions, disabledOptions);
                break;
            case ContextMenuGroup.IMAGE:
                addValidItems(items, IMAGE_GROUP, supportedOptions, disabledOptions);
                break;
            case ContextMenuGroup.VIDEO:
                addValidItems(items, VIDEO_GROUP, supportedOptions, disabledOptions);
                break;
            default:
                return;
        }

        if (items.isEmpty()) return;

        itemGroups.add(new Pair<>(titleResId, items));
    }

    private static void addValidItems(List<ContextMenuItem> validItems,
            List<? extends ContextMenuItem> allItems, Set<ContextMenuItem> supportedOptions,
            Set<ContextMenuItem> disabledOptions) {
        for (int i = 0; i < allItems.size(); i++) {
            ContextMenuItem item = allItems.get(i);
            if (supportedOptions.contains(item) && !disabledOptions.contains(item)) {
                assert !validItems.contains(item);
                validItems.add(item);
            }
        }
    }

    /**
     *  This works in the same way as {@link #addValidItems(List, List, Set, Set)} however the list
     *  given for example (a, b, c) will be added to list d, e f, as a, b, c, d, e, f not
     *  c, b, a, d, e, f.
     */
    private static void addValidItemsToFront(List<ContextMenuItem> validItems,
            List<? extends ContextMenuItem> allItems, Set<ContextMenuItem> supportedOptions,
            Set<ContextMenuItem> disabledOptions) {
        for (int i = allItems.size() - 1; i >= 0; i--) {
            ContextMenuItem item = allItems.get(i);
            if (supportedOptions.contains(item) && !disabledOptions.contains(item)) {
                assert !validItems.contains(item);
                validItems.add(0, item);
            }
        }
    }

    /**
     * Given a set of params. It creates a list of items that should not be accessible in specific
     * instances.
     * @param params The parameters used to create a list of items that should not be allowed.
     */
    private Set<ContextMenuItem> getDisabledOptions(ContextMenuParams params) {
        Set<ContextMenuItem> disabledOptions = new HashSet<>();
        if (!params.isAnchor()) {
            disabledOptions.addAll(LINK_GROUP);
        }
        if (!params.isImage()) {
            disabledOptions.addAll(IMAGE_GROUP);
        }
        if (!params.isVideo()) {
            disabledOptions.addAll(VIDEO_GROUP);
        }
        if (!MailTo.isMailTo(params.getLinkUrl())
                && !UrlUtilities.isTelScheme(params.getLinkUrl())) {
            disabledOptions.addAll(MESSAGE_GROUP);
        }

        if (params.isAnchor() && !mDelegate.isOpenInOtherWindowSupported()) {
            disabledOptions.add(ChromeContextMenuItem.OPEN_IN_OTHER_WINDOW);
        }

        if (mDelegate.isIncognito() || !mDelegate.isIncognitoSupported()) {
            disabledOptions.add(ChromeContextMenuItem.OPEN_IN_INCOGNITO_TAB);
        }

        if (params.getLinkText().trim().isEmpty() || params.isImage()) {
            disabledOptions.add(ChromeContextMenuItem.COPY_LINK_TEXT);
        }

        if (isEmptyUrl(params.getUrl()) || !UrlUtilities.isAcceptedScheme(params.getUrl())) {
            disabledOptions.add(ChromeContextMenuItem.OPEN_IN_OTHER_WINDOW);
            disabledOptions.add(ChromeContextMenuItem.OPEN_IN_NEW_TAB);
            disabledOptions.add(ChromeContextMenuItem.OPEN_IN_INCOGNITO_TAB);
        }

        if (MailTo.isMailTo(params.getLinkUrl())) {
            disabledOptions.add(ChromeContextMenuItem.COPY_LINK_TEXT);
            disabledOptions.add(ChromeContextMenuItem.COPY_LINK_ADDRESS);
            if (!mDelegate.supportsSendEmailMessage()) {
                disabledOptions.add(ChromeContextMenuItem.SEND_MESSAGE);
            }
            if (TextUtils.isEmpty(MailTo.parse(params.getLinkUrl()).getTo())
                    || !mDelegate.supportsAddToContacts()) {
                disabledOptions.add(ChromeContextMenuItem.ADD_TO_CONTACTS);
            }
            disabledOptions.add(ChromeContextMenuItem.CALL);
        } else if (UrlUtilities.isTelScheme(params.getLinkUrl())) {
            disabledOptions.add(ChromeContextMenuItem.COPY_LINK_TEXT);
            disabledOptions.add(ChromeContextMenuItem.COPY_LINK_ADDRESS);
            if (!mDelegate.supportsCall()) {
                disabledOptions.add(ChromeContextMenuItem.CALL);
            }
            if (!mDelegate.supportsSendTextMessage()) {
                disabledOptions.add(ChromeContextMenuItem.SEND_MESSAGE);
            }
            if (!mDelegate.supportsAddToContacts()) {
                disabledOptions.add(ChromeContextMenuItem.ADD_TO_CONTACTS);
            }
        }

        if (!UrlUtilities.isDownloadableScheme(params.getLinkUrl())) {
            disabledOptions.add(ChromeContextMenuItem.SAVE_LINK_AS);
        }

        boolean isSrcDownloadableScheme = UrlUtilities.isDownloadableScheme(params.getSrcUrl());
        if (params.isVideo()) {
            boolean saveableAndDownloadable = params.canSaveMedia() && isSrcDownloadableScheme;
            if (!saveableAndDownloadable) {
                disabledOptions.add(ChromeContextMenuItem.SAVE_VIDEO);
            }
        } else if (params.isImage() && params.imageWasFetchedLoFi()) {
            DataReductionProxyUma.previewsLoFiContextMenuAction(
                    DataReductionProxyUma.ACTION_LOFI_LOAD_IMAGE_CONTEXT_MENU_SHOWN);
            // All image context menu items other than "Load image," "Open original image in
            // new tab," and "Copy image URL" should be disabled on Lo-Fi images.
            disabledOptions.add(ChromeContextMenuItem.SAVE_IMAGE);
            disabledOptions.add(ChromeContextMenuItem.OPEN_IMAGE);
            disabledOptions.add(ChromeContextMenuItem.SEARCH_BY_IMAGE);
            disabledOptions.add(SHARE_IMAGE);
            recordSaveImageContextMenuResult(true, isSrcDownloadableScheme);
        } else if (params.isImage() && !params.imageWasFetchedLoFi()) {
            disabledOptions.add(ChromeContextMenuItem.LOAD_ORIGINAL_IMAGE);

            if (!isSrcDownloadableScheme) {
                disabledOptions.add(ChromeContextMenuItem.SAVE_IMAGE);
            }
            recordSaveImageContextMenuResult(false, isSrcDownloadableScheme);
            // Avoid showing open image option for same image which is already opened.
            if (mDelegate.getPageUrl().equals(params.getSrcUrl())) {
                disabledOptions.add(ChromeContextMenuItem.OPEN_IMAGE);
            }
            final TemplateUrlService templateUrlServiceInstance = getTemplateUrlService();
            final boolean isSearchByImageAvailable = isSrcDownloadableScheme
                    && templateUrlServiceInstance.isLoaded()
                    && templateUrlServiceInstance.isSearchByImageAvailable()
                    && templateUrlServiceInstance.getDefaultSearchEngineTemplateUrl() != null
                    && !LocaleManager.getInstance().needToCheckForSearchEnginePromo();

            if (!isSearchByImageAvailable) {
                disabledOptions.add(ChromeContextMenuItem.SEARCH_BY_IMAGE);
            }
        }

        if (mMode == ContextMenuMode.CUSTOM_TAB) {
            try {
                URI uri = new URI(params.getUrl());
                if (UrlUtilities.isInternalScheme(uri) || isEmptyUrl(params.getUrl())) {
                    disabledOptions.add(ChromeContextMenuItem.OPEN_IN_NEW_CHROME_TAB);
                    disabledOptions.add(ChromeContextMenuItem.OPEN_IN_CHROME_INCOGNITO_TAB);
                    disabledOptions.add(ChromeContextMenuItem.OPEN_IN_BROWSER_ID);
                } else if (ChromePreferenceManager.getInstance().getCachedChromeDefaultBrowser()) {
                    disabledOptions.add(ChromeContextMenuItem.OPEN_IN_BROWSER_ID);
                    if (!mDelegate.isIncognitoSupported()) {
                        disabledOptions.add(ChromeContextMenuItem.OPEN_IN_CHROME_INCOGNITO_TAB);
                    }
                } else {
                    disabledOptions.add(ChromeContextMenuItem.OPEN_IN_NEW_CHROME_TAB);
                    disabledOptions.add(ChromeContextMenuItem.OPEN_IN_CHROME_INCOGNITO_TAB);
                }
            } catch (URISyntaxException e) {
                return disabledOptions;
            }
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CUSTOM_CONTEXT_MENU)) {
            disabledOptions.add(ChromeContextMenuItem.COPY_LINK_TEXT);
        }

        return disabledOptions;
    }

    @Override
    public boolean onItemSelected(ContextMenuHelper helper, ContextMenuParams params, int itemId) {
        if (itemId == R.id.contextmenu_open_in_other_window) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_OTHER_WINDOW);
            mDelegate.onOpenInOtherWindow(params.getUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_new_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_NEW_TAB);
            mDelegate.onOpenInNewTab(params.getUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_INCOGNITO_TAB);
            mDelegate.onOpenInNewIncognitoTab(params.getUrl());
        } else if (itemId == R.id.contextmenu_open_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IMAGE);
            mDelegate.onOpenImageUrl(params.getSrcUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_new_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IMAGE_IN_NEW_TAB);
            mDelegate.onOpenImageInNewTab(params.getSrcUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_load_original_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.LOAD_ORIGINAL_IMAGE);
            DataReductionProxyUma.previewsLoFiContextMenuAction(
                    DataReductionProxyUma.ACTION_LOFI_LOAD_IMAGE_CONTEXT_MENU_CLICKED);
            if (!mDelegate.wasLoadOriginalImageRequestedForPageLoad()) {
                DataReductionProxyUma.previewsLoFiContextMenuAction(
                        DataReductionProxyUma.ACTION_LOFI_LOAD_IMAGE_CONTEXT_MENU_CLICKED_ON_PAGE);
            }
            mDelegate.onLoadOriginalImage();
        } else if (itemId == R.id.contextmenu_copy_link_address) {
            ContextMenuUma.record(params, ContextMenuUma.Action.COPY_LINK_ADDRESS);
            mDelegate.onSaveToClipboard(
                    params.getUnfilteredLinkUrl(), ContextMenuItemDelegate.ClipboardType.LINK_URL);
        } else if (itemId == R.id.contextmenu_call) {
            ContextMenuUma.record(params, ContextMenuUma.Action.CALL);
            mDelegate.onCall(params.getLinkUrl());
        } else if (itemId == R.id.contextmenu_send_message) {
            if (MailTo.isMailTo(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.SEND_EMAIL);
                mDelegate.onSendEmailMessage(params.getLinkUrl());
            } else if (UrlUtilities.isTelScheme(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.SEND_TEXT_MESSAGE);
                mDelegate.onSendTextMessage(params.getLinkUrl());
            }
        } else if (itemId == R.id.contextmenu_add_to_contacts) {
            ContextMenuUma.record(params, ContextMenuUma.Action.ADD_TO_CONTACTS);
            mDelegate.onAddToContacts(params.getLinkUrl());
        } else if (itemId == R.id.contextmenu_copy) {
            if (MailTo.isMailTo(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.COPY_EMAIL_ADDRESS);
                mDelegate.onSaveToClipboard(MailTo.parse(params.getLinkUrl()).getTo(),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            } else if (UrlUtilities.isTelScheme(params.getLinkUrl())) {
                ContextMenuUma.record(params, ContextMenuUma.Action.COPY_PHONE_NUMBER);
                mDelegate.onSaveToClipboard(UrlUtilities.getTelNumber(params.getLinkUrl()),
                        ContextMenuItemDelegate.ClipboardType.LINK_URL);
            }
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            ContextMenuUma.record(params, ContextMenuUma.Action.COPY_LINK_TEXT);
            mDelegate.onSaveToClipboard(
                    params.getLinkText(), ContextMenuItemDelegate.ClipboardType.LINK_TEXT);
        } else if (itemId == R.id.contextmenu_save_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SAVE_IMAGE);
            if (mDelegate.startDownload(params.getSrcUrl(), false)) {
                helper.startContextMenuDownload(
                        false, mDelegate.isDataReductionProxyEnabledForURL(params.getSrcUrl()));
            }
        } else if (itemId == R.id.contextmenu_save_video) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SAVE_VIDEO);
            if (mDelegate.startDownload(params.getSrcUrl(), false)) {
                helper.startContextMenuDownload(false, false);
            }
        } else if (itemId == R.id.contextmenu_save_link_as) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SAVE_LINK);
            String url = params.getUnfilteredLinkUrl();
            if (mDelegate.startDownload(url, true)) {
                ContextMenuUma.recordSaveLinkTypes(url);
                helper.startContextMenuDownload(true, false);
            }
        } else if (itemId == R.id.contextmenu_share_link) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SHARE_LINK);
            ShareParams linkShareParams =
                    new ShareParams.Builder(helper.getActivity(), params.getUrl(), params.getUrl())
                            .setShareDirectly(false)
                            .setSaveLastUsed(true)
                            .build();
            ShareHelper.share(linkShareParams);
        } else if (itemId == R.id.contextmenu_search_by_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SEARCH_BY_IMAGE);
            helper.searchForImage();
        } else if (itemId == R.id.contextmenu_share_image) {
            ContextMenuUma.record(params, ContextMenuUma.Action.SHARE_IMAGE);
            helper.shareImage();
        } else if (itemId == R.id.contextmenu_open_in_chrome) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_CHROME);
            mDelegate.onOpenInChrome(params.getUrl(), params.getPageUrl());
        } else if (itemId == R.id.contextmenu_open_in_new_chrome_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_NEW_CHROME_TAB);
            mDelegate.onOpenInNewChromeTabFromCCT(params.getUrl(), false);
        } else if (itemId == R.id.contextmenu_open_in_chrome_incognito_tab) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_CHROME_INCOGNITO_TAB);
            mDelegate.onOpenInNewChromeTabFromCCT(params.getUrl(), true);
        } else if (itemId == R.id.contextmenu_open_in_browser_id) {
            ContextMenuUma.record(params, ContextMenuUma.Action.OPEN_IN_BROWSER);
            mDelegate.onOpenInDefaultBrowser(params.getUrl());
        } else {
            assert false;
        }

        return true;
    }

    /**
     * @return The service that handles TemplateUrls.
     */
    protected TemplateUrlService getTemplateUrlService() {
        return TemplateUrlService.getInstance();
    }

    /**
     * Checks whether a url is empty or blank.
     * @param url The url need to be checked.
     * @return True if the url is empty or "about:blank".
     */
    private static boolean isEmptyUrl(String url) {
        return TextUtils.isEmpty(url) || url.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    /**
     * Record the UMA related to save image context menu option.
     * @param wasFetchedLoFi The image was fetched Lo-Fi.
     * @param isDownloadableScheme The image is downloadable.
     */
    private void recordSaveImageContextMenuResult(
            boolean wasFetchedLoFi, boolean isDownloadableScheme) {
        if (!BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()) {
            return;
        }

        ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.LOADED);

        if (wasFetchedLoFi) {
            ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.FETCHED_LOFI);
            return;
        }

        if (!isDownloadableScheme) {
            ContextMenuUma.recordSaveImageUma(ContextMenuUma.TypeSaveImage.NOT_DOWNLOADABLE);
        }
    }
}
