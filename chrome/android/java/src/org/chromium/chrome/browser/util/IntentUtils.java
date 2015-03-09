// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;

import java.util.ArrayList;
import java.util.List;

/**
 * Utilities dealing with extracting information from intents.
 */
public class IntentUtils {

    /**
     * Retrieves a list of components that would handle the given intent.
     * @param context The application context.
     * @param intent The intent which we are interested in.
     * @return The list of component names.
     */
    public static List<ComponentName> getIntentHandlers(Context context, Intent intent) {
        List<ResolveInfo> list = context.getPackageManager().queryIntentActivities(intent, 0);
        List<ComponentName> nameList = new ArrayList<ComponentName>();
        for (ResolveInfo r : list) {
            nameList.add(new ComponentName(r.activityInfo.packageName, r.activityInfo.name));
        }
        return nameList;
    }
}
