// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.common.Referrer;

/**
 * A delegate responsible for taking actions based on context menu selections.
 */
public interface ChromeContextMenuItemDelegate {
    /**
     * @return Whether or not this context menu is being shown for an incognito
     *     {@link ContentViewCore}.
     */
    boolean isIncognito();

    /**
     * @return Whether or not the current application can show incognito tabs.
     */
    boolean isIncognitoSupported();

    /**
     * @return Whether or not the context menu should give the user the chance to show the original
     *         image.
     */
    boolean canLoadOriginalImage();

    /**
     * Called when the context menu is trying to start a download.
     * @param url Url of the download item.
     * @param isLink Whether or not the download is a link (as opposed to an image/video).
     * @return       Whether or not a download should actually be started.
     */
    boolean startDownload(String url, boolean isLink);

    /**
     * Called when the {@code url} should be opened in a new tab with the same incognito state as
     * the current {@link Tab}.
     * @param url The URL to open.
     */
    void onOpenInNewTab(String url, Referrer referrer);

    /**
     * Called when the {@code url} should be opened in a new incognito tab.
     * @param url The URL to open.
     */
    void onOpenInNewIncognitoTab(String url);

    /**
     * Called when the {@code url} is of an image and should be opened in the same tab.
     * @param url The image URL to open.
     */
    void onOpenImageUrl(String url, Referrer referrer);

    /**
     * Called when the {@code url} is of an image and should be opened in a new tab.
     * @param url The image URL to open.
     */
    void onOpenImageInNewTab(String url, Referrer referrer);

    /**
     * Called when the {@code text} should be saved to the clipboard.
     * @param text  The text to save to the clipboard.
     * @param isUrl Whether or not the text is a URL.
     */
    void onSaveToClipboard(String text, boolean isUrl);

    /**
     * Called when the {@code url} is of an image and a link to the image should be saved to the
     * clipboard.
     * @param url The image URL.
     */
    void onSaveImageToClipboard(String url);

    /**
     * Called when a search by image should be performed in a new tab.
     */
    void onSearchByImageInNewTab();

   /**
    * @return page url.
    */
    String getPageUrl();
}
