// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab.TopControlsVisibilityDelegate;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned
 * by a {@link FullScreenActivity}.
 */
public class FullScreenDelegateFactory extends TabDelegateFactory {
    private static class FullScreenTabWebContentsDelegateAndroid
            extends TabWebContentsDelegateAndroid {

        public FullScreenTabWebContentsDelegateAndroid(Tab tab) {
            super(tab);
        }

        @Override
        public void activateContents() {
            Context context = mTab.getWindowAndroid().getContext().get();
            if (!(context instanceof WebappActivity)) return;

            WebappInfo webappInfo = ((WebappActivity) context).getWebappInfo();
            String url = webappInfo.uri().toString();

            // Create an Intent that will be fired toward the WebappLauncherActivity, which in turn
            // will fire an Intent to launch the correct WebappActivity.  On L+ this could probably
            // be changed to call AppTask.moveToFront(), but for backwards compatibility we relaunch
            // it the hard way.
            Intent intent = new Intent();
            intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
            intent.setPackage(context.getPackageName());
            webappInfo.setWebappIntentExtras(intent);

            intent.putExtra(ShortcutHelper.EXTRA_MAC, ShortcutHelper.getEncodedMac(context, url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.getApplicationContext().startActivity(intent);
        }
    }

    @Override
    public FullScreenTabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new FullScreenTabWebContentsDelegateAndroid(tab);
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(Tab tab) {
        return new ChromeContextMenuPopulator(new TabContextMenuItemDelegate(tab),
                ChromeContextMenuPopulator.FULLSCREEN_TAB_MODE);
    }

    @Override
    public TopControlsVisibilityDelegate createTopControlsVisibilityDelegate(Tab tab) {
        return new TopControlsVisibilityDelegate(tab) {
            @Override
            public boolean isHidingTopControlsEnabled() {
                return !isShowingTopControlsEnabled();
            }
        };
    }
}
