// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_activity_glue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserData;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Takes care of reparenting a Tab object from one Activity to another.
 */
public class ReparentingTask implements UserData {
    private static final Class<ReparentingTask> USER_DATA_KEY = ReparentingTask.class;

    private final Tab mTab;

    /**
     * @param tab {@link Tab} object.
     * @return {@link ReparentingTask} object for a given {@link Tab}. Creates one
     *         if not present.
     */
    public static ReparentingTask from(Tab tab) {
        ReparentingTask reparentingTask = get(tab);
        if (reparentingTask == null) {
            reparentingTask =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new ReparentingTask(tab));
        }
        return reparentingTask;
    }

    @Nullable
    private static ReparentingTask get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private ReparentingTask(Tab tab) {
        mTab = tab;
    }

    /**
     * Begins the tab reparenting process. Detaches the tab from its current activity and fires
     * an Intent to reparent the tab into its new host activity.
     *
     * @param context {@link Context} object used to start a new activity.
     * @param intent An optional intent with the desired component, flags, or extras to use when
     *               launching the new host activity. This intent's URI and action will be
     *               overridden. This may be null if no intent customization is needed.
     * @param startActivityOptions Options to pass to {@link Activity#startActivity(Intent, Bundle)}
     * @param finalizeCallback A callback that will be called after the tab is attached to the new
     *                         host activity in {@link #attachAndFinishReparenting}.
     * @return Whether reparenting succeeded. If false, the tab was not removed and the intent was
     *         not fired.
     */
    public boolean begin(Context context, Intent intent, Bundle startActivityOptions,
            Runnable finalizeCallback) {
        if (intent == null) intent = new Intent();
        if (intent.getComponent() == null) {
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        }
        intent.setAction(Intent.ACTION_VIEW);
        if (TextUtils.isEmpty(intent.getDataString())) intent.setData(Uri.parse(mTab.getUrl()));
        if (mTab.isIncognito()) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        }
        IntentHandler.addTrustedIntentExtras(intent);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_REPARENTING)) {
            // Add the tab to AsyncTabParamsManager before removing it from the current model to
            // ensure the global count of tabs is correct. See https://crbug.com/611806.
            intent.putExtra(IntentHandler.EXTRA_TAB_ID, mTab.getId());
            AsyncTabParamsManager.add(
                    mTab.getId(), new TabReparentingParams(mTab, intent, finalizeCallback));

            detach(mTab);
        }

        context.startActivity(intent, startActivityOptions);
        return true;
    }

    /**
     * Detaches a tab from its current activity if any.
     *
     * In details, this function:
     * - Removes the tab from its current {@link TabModelSelector}, effectively severing
     *   the {@link Activity} to {@link Tab} link.
     */
    public static void detach(Tab tab) {
        // TODO(yusufo): We can't call tab.updateWindowAndroid that sets |mWindowAndroid| to null
        // because many code paths (including navigation) expect the tab to always be associated
        // with an activity, and will crash. crbug.com/657007
        WebContents webContents = tab.getWebContents();
        if (webContents != null) webContents.setTopLevelNativeWindow(null);

        // TabModelSelector of this Tab, if present, gets notified to remove the tab from
        // the TabModel it belonged to.
        tab.updateAttachment(null, null);
    }

    /**
     * Finishes the tab reparenting process. Attaches this tab to a new activity, and updates
     * the tab and related objects to reference it. This updates many delegates inside the tab
     * and {@link WebContents} both on java and native sides.
     *
     * @param activity A new {@link ChromeActivity} to attach this Tab instance to.
     * @param tabDelegateFactory The new delegate factory this tab should be using.
     * @param finalizeCallback A Callback to be called after the Tab has been reparented.
     */
    public void finish(ChromeActivity activity, TabDelegateFactory tabDelegateFactory,
            @Nullable Runnable finalizeCallback) {
        activity.getCompositorViewHolder().prepareForTabReparenting();
        attach(activity.getWindowAndroid(), tabDelegateFactory);
        ((TabImpl) mTab).setIsTabStateDirty(true);
        if (finalizeCallback != null) finalizeCallback.run();
    }

    /**
     * Attaches the tab to the new activity and updates the tab and related objects to reference the
     * new activity. This updates many delegates inside the tab and {@link WebContents} both on
     * java and native sides.
     *
     * @param window A new {@link WindowAndroid} to attach the tab to.
     * @param tabDelegateFactory  The new delegate factory this tab should be using.
     */
    private void attach(WindowAndroid window, TabDelegateFactory tabDelegateFactory) {
        assert TabImpl.isDetached(mTab);
        mTab.updateAttachment(window, tabDelegateFactory);
        ReparentingTaskJni.get().attachTab(mTab.getWebContents());
    }

    @NativeMethods
    interface Natives {
        void attachTab(WebContents webContents);
    }
}
