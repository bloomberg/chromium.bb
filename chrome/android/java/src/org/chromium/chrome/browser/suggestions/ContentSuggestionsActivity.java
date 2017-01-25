// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.SynchronousInitializationActivity;

import java.lang.ref.WeakReference;

/**
 * Experimental activity to show content suggestions outside of the New Tab Page.
 */
public class ContentSuggestionsActivity extends SynchronousInitializationActivity {
    private static WeakReference<ChromeActivity> sCallerActivity;

    private SuggestionsBottomSheetContent mBottomSheetContent;

    public static void launch(ChromeActivity activity) {
        sCallerActivity = new WeakReference<>(activity);

        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClass(activity, ContentSuggestionsActivity.class);
        activity.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        assert ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SUGGESTIONS_STANDALONE_UI);

        // TODO(dgn): properly handle retrieving the tab information, or the base activity being
        // destroyed. (https://crbug.com/677672)
        ChromeActivity activity = sCallerActivity.get();
        if (activity == null) throw new IllegalStateException();

        // TODO(dgn): Because the passed activity here is the Chrome one and not the one showing
        // the surface, some things, like closing the context menu will not work as they would
        // affect the wrong one.
        mBottomSheetContent = new SuggestionsBottomSheetContent(
                activity, activity.getActivityTab(), activity.getTabModelSelector());
        setContentView(mBottomSheetContent.getScrollingContentView());
    }

    @Override
    public void onContextMenuClosed(Menu menu) {
        mBottomSheetContent.getContextMenuManager().onContextMenuClosed();
    }

    @Override
    protected void onDestroy() {
        mBottomSheetContent.destroy();
        super.onDestroy();
    }
}
