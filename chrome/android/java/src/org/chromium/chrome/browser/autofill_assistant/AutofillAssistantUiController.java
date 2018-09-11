// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantUiController implements BottomBarController.Client {
    private final long mUiControllerAndroid;
    private final BottomBarController mBottomBarController;

    /**
     * Construct Autofill Assistant UI controller.
     *
     * @param activity The CustomTabActivity of the controller associated with.
     */
    public AutofillAssistantUiController(CustomTabActivity activity) {
        // TODO(crbug.com/806868): Implement corresponding UI.
        Tab activityTab = activity.getActivityTab();
        mUiControllerAndroid = nativeInit(activityTab.getWebContents());

        // Stop Autofill Assistant when the tab is detached from the activity.
        activityTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (!isAttached) {
                    activityTab.removeObserver(this);
                    nativeDestroy(mUiControllerAndroid);
                }
            }
        });

        // Stop Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                currentTabModel.removeObserver(this);

                // Assume newly selected tab is always different from the last one.
                nativeDestroy(mUiControllerAndroid);
                // TODO(crbug.com/806868): May start a new Autofill Assistant instance for the newly
                // selected Tab.
            }
        });

        mBottomBarController = new BottomBarController(activity, this);
    }

    @Override
    public void onScriptSelected(String scriptPath) {
        nativeOnScriptSelected(mUiControllerAndroid, scriptPath);
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
        mBottomBarController.showStatusMessage(message);
    }

    @CalledByNative
    private void onShowOverlay() {
        // TODO(crbug.com/806868): Implement corresponding UI.
    }

    @CalledByNative
    private void onHideOverlay() {
        // TODO(crbug.com/806868): Implement corresponding UI.
    }

    @CalledByNative
    private void onUpdateScripts(String[] scriptNames, String[] scriptPaths) {
        List<BottomBarController.ScriptHandle> scriptHandles = new ArrayList<>();
        // Note that scriptNames and scriptPaths are one-on-one matched by index.
        for (int i = 0; i < scriptNames.length; i++) {
            scriptHandles.add(new BottomBarController.ScriptHandle(scriptNames[i], scriptPaths[i]));
        }
        mBottomBarController.updateScripts(scriptHandles);
    }

    // native methods.
    private native long nativeInit(WebContents webContents);
    private native void nativeDestroy(long nativeUiControllerAndroid);
    private native void nativeOnScriptSelected(long nativeUiControllerAndroid, String scriptPath);
}
