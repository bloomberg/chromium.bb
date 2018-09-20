// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantUiController implements AutofillAssistantUiDelegate.Client {
    /** Prefix for Intent extras relevant to this feature. */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    private final long mUiControllerAndroid;
    private final AutofillAssistantUiDelegate mUiDelegate;

    /**
     * Construct Autofill Assistant UI controller.
     *
     * @param activity The CustomTabActivity of the controller associated with.
     */
    public AutofillAssistantUiController(CustomTabActivity activity) {
        Tab activityTab = activity.getActivityTab();

        Map<String, String> parameters = extractParameters(activity.getInitialIntent().getExtras());
        // TODO(crbug.com/806868): Treat parameter
        // org.chromium.chrome.browser.autofill_assistant.ENABLED specially, and disable autofill
        // assistant if it is false or unset.

        mUiControllerAndroid = nativeInit(activityTab.getWebContents(),
                parameters.keySet().toArray(new String[parameters.size()]),
                parameters.values().toArray(new String[parameters.size()]));

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

        mUiDelegate = new AutofillAssistantUiDelegate(activity, this);
    }

    @Override
    public void onDismiss() {
        nativeDestroy(mUiControllerAndroid);
    }

    @Override
    public void onScriptSelected(String scriptPath) {
        nativeOnScriptSelected(mUiControllerAndroid, scriptPath);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(Bundle extras) {
        Map<String, String> result = new HashMap<>();
        for (String key : extras.keySet()) {
            if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                result.put(key.substring(INTENT_EXTRA_PREFIX.length()), extras.get(key).toString());
            }
        }
        return result;
    }

    @Override
    public void onClickOverlay() {
        // TODO(crbug.com/806868): Notify native side.
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
        mUiDelegate.showStatusMessage(message);
    }

    @CalledByNative
    private void onShowOverlay() {
        mUiDelegate.showOverlay();
    }

    @CalledByNative
    private void onHideOverlay() {
        mUiDelegate.hideOverlay();
    }

    @CalledByNative
    private void onUpdateScripts(String[] scriptNames, String[] scriptPaths) {
        List<AutofillAssistantUiDelegate.ScriptHandle> scriptHandles = new ArrayList<>();
        // Note that scriptNames and scriptPaths are one-on-one matched by index.
        for (int i = 0; i < scriptNames.length; i++) {
            scriptHandles.add(
                    new AutofillAssistantUiDelegate.ScriptHandle(scriptNames[i], scriptPaths[i]));
        }
        mUiDelegate.updateScripts(scriptHandles);
    }

    // native methods.
    private native long nativeInit(
            WebContents webContents, String[] parameterNames, String[] parameterValues);
    private native void nativeDestroy(long nativeUiControllerAndroid);
    private native void nativeOnScriptSelected(long nativeUiControllerAndroid, String scriptPath);
}
