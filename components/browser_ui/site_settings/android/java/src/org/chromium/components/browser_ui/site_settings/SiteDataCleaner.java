// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

/**
 * Encapsulates clearing the data of {@link Website}s.
 * Requires native library to be initialized.
 */
public class SiteDataCleaner {
    /**
     * Clears the data of the specified site.
     * @param finishCallback is called when finished.
     */
    public void clearData(
            BrowserContextHandle browserContextHandle, Website site, Runnable finishCallback) {
        String origin = site.getAddress().getOrigin();
        WebsitePreferenceBridgeJni.get().clearCookieData(browserContextHandle, origin);
        WebsitePreferenceBridgeJni.get().clearBannerData(browserContextHandle, origin);
        WebsitePreferenceBridgeJni.get().clearMediaLicenses(browserContextHandle, origin);

        // Clear the permissions.
        for (@ContentSettingException.Type int type = 0;
                type < ContentSettingException.Type.NUM_ENTRIES; type++) {
            site.setContentSettingPermission(
                    browserContextHandle, type, ContentSettingValues.DEFAULT);
        }
        for (@PermissionInfo.Type int type = 0; type < PermissionInfo.Type.NUM_ENTRIES; type++) {
            site.setPermission(browserContextHandle, type, ContentSettingValues.DEFAULT);
        }

        for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
            info.revoke(browserContextHandle);
        }

        site.clearAllStoredData(browserContextHandle, finishCallback::run);
    }
}
