// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.net.MailTo;
import android.os.Build;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.MenuInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;

/**
 * A {@link ContextMenuPopulator} used for showing the default Chrome context menu.
 */
public class ChromeContextMenuPopulator implements ContextMenuPopulator {
    private final ChromeContextMenuItemDelegate mDelegate;
    private MenuInflater mMenuInflater;
    private static final String BLANK_URL = "about:blank";

    /**
     * Builds a {@link ChromeContextMenuPopulator}.
     * @param delegate The {@link ChromeContextMenuItemDelegate} that will be notified with actions
     *                 to perform when menu items are selected.
     */
    public ChromeContextMenuPopulator(ChromeContextMenuItemDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public boolean shouldShowContextMenu(ContextMenuParams params) {
        return params != null && (params.isAnchor() || params.isEditable() || params.isImage()
                || params.isSelectedText() || params.isVideo() || params.isCustomMenu());
    }

    @Override
    public void buildContextMenu(ContextMenu menu, Context context, ContextMenuParams params) {
        if (!TextUtils.isEmpty(params.getLinkUrl()) && !params.getLinkUrl().equals(BLANK_URL))
                menu.setHeaderTitle(params.getLinkUrl());

        if (mMenuInflater == null) mMenuInflater = new MenuInflater(context);

        mMenuInflater.inflate(R.menu.chrome_context_menu, menu);

        menu.setGroupVisible(R.id.contextmenu_group_anchor, params.isAnchor());
        menu.setGroupVisible(R.id.contextmenu_group_image, params.isImage());
        menu.setGroupVisible(R.id.contextmenu_group_video, params.isVideo());

        if (mDelegate.isIncognito() || !mDelegate.isIncognitoSupported()) {
            menu.findItem(R.id.contextmenu_open_in_incognito_tab).setVisible(false);
        }

        if (params.getLinkText().trim().isEmpty()) {
            menu.findItem(R.id.contextmenu_copy_link_text).setVisible(false);
        }

        if (MailTo.isMailTo(params.getLinkUrl())) {
            menu.findItem(R.id.contextmenu_copy_link_address_text).setVisible(false);
        } else {
            menu.findItem(R.id.contextmenu_copy_email_address).setVisible(false);
        }

        menu.findItem(R.id.contextmenu_save_link_as).setVisible(
                UrlUtilities.isDownloadableScheme(params.getLinkUrl()));

        if (params.isVideo()) {
            menu.findItem(R.id.contextmenu_save_video).setVisible(
                    UrlUtilities.isDownloadableScheme(params.getSrcUrl()));
        } else if (params.isImage()) {
            menu.findItem(R.id.contextmenu_save_image).setVisible(
                    UrlUtilities.isDownloadableScheme(params.getSrcUrl()));

            if (mDelegate.canLoadOriginalImage()) {
                menu.findItem(R.id.contextmenu_open_image_in_new_tab).setVisible(false);
            } else {
                menu.findItem(R.id.contextmenu_open_original_image_in_new_tab).setVisible(false);
            }

            // Avoid showing open image option for same image which is already opened.
            if (mDelegate.getPageUrl().equals(params.getSrcUrl())) {
                menu.findItem(R.id.contextmenu_open_image).setVisible(false);
            }
            final TemplateUrlService templateUrlServiceInstance = TemplateUrlService.getInstance();
            final boolean isSearchByImageAvailable =
                    UrlUtilities.isDownloadableScheme(params.getSrcUrl()) &&
                            templateUrlServiceInstance.isLoaded() &&
                            templateUrlServiceInstance.isSearchByImageAvailable() &&
                            templateUrlServiceInstance.getDefaultSearchEngineTemplateUrl() != null;

            menu.findItem(R.id.contextmenu_search_by_image).setVisible(isSearchByImageAvailable);
            if (isSearchByImageAvailable) {
                menu.findItem(R.id.contextmenu_search_by_image).setTitle(
                        context.getString(R.string.contextmenu_search_web_for_image,
                                TemplateUrlService.getInstance().
                                        getDefaultSearchEngineTemplateUrl().getShortName()));
            }

            menu.findItem(R.id.contextmenu_copy_image).setVisible(
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN);
        }
    }

    @Override
    public boolean onItemSelected(ContextMenuHelper helper, ContextMenuParams params, int itemId) {
        if (itemId == R.id.contextmenu_open_in_new_tab) {
            mDelegate.onOpenInNewTab(params.getLinkUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_in_incognito_tab) {
            mDelegate.onOpenInNewIncognitoTab(params.getLinkUrl());
        } else if (itemId == R.id.contextmenu_open_image) {
            mDelegate.onOpenImageUrl(params.getSrcUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_open_image_in_new_tab ||
                itemId == R.id.contextmenu_open_original_image_in_new_tab) {
            mDelegate.onOpenImageInNewTab(params.getSrcUrl(), params.getReferrer());
        } else if (itemId == R.id.contextmenu_copy_link_address_text) {
            mDelegate.onSaveToClipboard(params.getUnfilteredLinkUrl(), true);
        } else if (itemId == R.id.contextmenu_copy_email_address) {
            mDelegate.onSaveToClipboard(MailTo.parse(params.getLinkUrl()).getTo(), false);
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            mDelegate.onSaveToClipboard(params.getLinkText(), false);
        } else if (itemId == R.id.contextmenu_save_image ||
                itemId == R.id.contextmenu_save_video) {
            if (mDelegate.startDownload(params.getSrcUrl(), false)) {
                helper.startContextMenuDownload(false);
            }
        } else if (itemId == R.id.contextmenu_save_link_as) {
            if (mDelegate.startDownload(params.getUnfilteredLinkUrl(), true)) {
                helper.startContextMenuDownload(true);
            }
        } else if (itemId == R.id.contextmenu_search_by_image) {
            mDelegate.onSearchByImageInNewTab();
        } else if (itemId == R.id.contextmenu_copy_image) {
            mDelegate.onSaveImageToClipboard(params.getSrcUrl());
        } else if (itemId == R.id.contextmenu_copy_image_url) {
            mDelegate.onSaveToClipboard(params.getSrcUrl(), true);
        } else {
            assert false;
        }

        return true;
    }
}
