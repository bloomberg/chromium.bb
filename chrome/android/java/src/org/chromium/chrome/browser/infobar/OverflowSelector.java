// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.PopupMenu;
import android.widget.PopupMenu.OnMenuItemClickListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.widget.ButtonCompat;

/**
 * Button with text "More", which opens a popup with two elements: "Learn more" link to help center
 * and link named "Settings" which opens general settings page. This element is used by Smart Lock
 * infobars.
 */
public class OverflowSelector {
    private OverflowSelector() {}

    /**
     * Creates a material style borderless button with text "More" which opens a popup menu,
     * which allows to go to help center article or Settings.
     */
    public static Button createOverflowSelector(Context context) {
        Button b = ButtonCompat.createBorderlessButton(context);
        b.setText(context.getString(R.string.more));
        b.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                OverflowSelector.showMorePopup(view);
            }
        });
        return b;
    }

    /** Pops up menu with two items: "Settings" and "Learn more" when user clicks "More" button. */
    private static void showMorePopup(View v) {
        final Context context = v.getContext();
        PopupMenu popup = new PopupMenu(context, v);
        popup.setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                return OverflowSelector.processMenuItem(item, context);
            }
        });
        popup.inflate(R.menu.overflow_selector_for_smart_lock_infobars);
        popup.show();
    }

    private static boolean processMenuItem(MenuItem item, Context context) {
        if (item.getItemId() == R.id.settings) {
            // TODO(melandory) When Smart Lock setting will be implemented for Chrome on Android
            // this code should open settings fragment with Smart Lock parameters.
            PreferencesLauncher.launchSettingsPage(context, null);
            return true;
        }
        // TODO(melandory): Learn more should open link to help center
        // article which is not ready yet.
        return false;
    }
}
