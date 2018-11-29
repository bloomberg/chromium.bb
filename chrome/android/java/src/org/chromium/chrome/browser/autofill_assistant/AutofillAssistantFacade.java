// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;

import java.util.HashMap;
import java.util.Map;

/** Facade for starting Autofill Assistant on a custom tab. */
public class AutofillAssistantFacade {
    /** Prefix for Intent extras relevant to this feature. */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";


    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /** Returns true if all conditions are satisfied to start Autofill Assistant. */
    public static boolean isConfigured(@Nullable Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED)
                && !AutofillAssistantStudy.getUrl().isEmpty()
                && AutofillAssistantPreferencesUtil.canShowAutofillAssistant();
    }

    /** Starts Autofill Assistant on the given {@code activity}. */
    public static void start(ChromeActivity activity) {
        Map<String, String> parameters = extractParameters(activity.getInitialIntent().getExtras());
        parameters.remove(PARAMETER_ENABLED);
        if (!AutofillAssistantPreferencesUtil.getSkipInitScreenPreference()) {
            FirstRunScreen.show(activity, (result) -> {
                if (result) initiateAutofillAssistant(activity, parameters);
            });
            return;
        }

        if (AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn()
                && AutofillAssistantPreferencesUtil.getSkipInitScreenPreference()) {
            initiateAutofillAssistant(activity, parameters);
        }
        // We don't have consent to start Autofill Assistant and cannot show initial screen.
        // Do nothing.
    }

    /**
     * Instantiates all essential Autofill Assistant components and starts it.
     */
    private static void initiateAutofillAssistant(
            ChromeActivity activity, Map<String, String> parameters) {
        AutofillAssistantUiController controller =
                new AutofillAssistantUiController(activity, parameters);
        UiDelegateHolder delegateHolder = new UiDelegateHolder(
                controller, new AutofillAssistantUiDelegate(activity, controller));
        initTabObservers(activity, delegateHolder);

        controller.init(delegateHolder, Details.makeFromParameters(parameters));
    }

    private static void initTabObservers(ChromeActivity activity, UiDelegateHolder delegateHolder) {
        // Shut down Autofill Assistant when the tab is detached from the activity.
        //
        // Note: For now this logic assumes that |activity| is a CustomTabActivity.
        Tab activityTab = activity.getActivityTab();
        activityTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (!isAttached) {
                    activityTab.removeObserver(this);
                    delegateHolder.shutdown();
                }
            }
        });

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                currentTabModel.removeObserver(this);
                delegateHolder.giveUp();
            }
        });
    }

    /** Return the value if the given boolean parameter from the extras. */
    private static boolean getBooleanParameter(@Nullable Bundle extras, String parameterName) {
        return extras != null && extras.getBoolean(INTENT_EXTRA_PREFIX + parameterName, false);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(@Nullable Bundle extras) {
        Map<String, String> result = new HashMap<>();
        if (extras != null) {
            for (String key : extras.keySet()) {
                if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                    result.put(key.substring(INTENT_EXTRA_PREFIX.length()),
                            extras.get(key).toString());
                }
            }
        }
        return result;
    }
}
