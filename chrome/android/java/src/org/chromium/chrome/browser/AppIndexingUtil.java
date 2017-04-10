// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.webkit.URLUtil;

import org.chromium.base.SysUtils;
import org.chromium.blink.mojom.document_metadata.CopylessPaste;
import org.chromium.blink.mojom.document_metadata.WebPage;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceProvider;

/**
 * This is the top-level CopylessPaste metadata extraction for AppIndexing.
 */
public class AppIndexingUtil {
    public static void extractCopylessPasteMetadata(final Tab tab) {
        String url = tab.getUrl();
        boolean isHttpOrHttps = URLUtil.isHttpsUrl(url) || URLUtil.isHttpUrl(url);
        if (SysUtils.isLowEndDevice() || tab.isIncognito()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.COPYLESS_PASTE)
                || !isHttpOrHttps) {
            return;
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        RenderFrameHost mainFrame = webContents.getMainFrame();
        if (mainFrame == null) return;

        InterfaceProvider interfaces = mainFrame.getRemoteInterfaces();
        if (interfaces == null) return;

        CopylessPaste copylesspaste = interfaces.getInterface(CopylessPaste.MANAGER);
        copylesspaste.getEntities(new CopylessPaste.GetEntitiesResponse() {
            @Override
            public void call(WebPage webpage) {
                if (webpage == null) return;
                AppIndexingReporter.getInstance().reportWebPage(webpage);
            }
        });
    }
}
