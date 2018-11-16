// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/** Facade for starting Autofill Assistant on a custom tab. */
public class AutofillAssistantFacade {
    private static final String RFC_3339_FORMAT_WITHOUT_TIMEZONE = "yyyy'-'MM'-'dd'T'HH':'mm':'ss";

    /** Prefix for Intent extras relevant to this feature. */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Autofill Assistant Study name. */
    private static final String STUDY_NAME = "AutofillAssistant";

    /** Variation url parameter name. */
    private static final String URL_PARAMETER_NAME = "url";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /** Returns true if all conditions are satisfied to start Autofill Assistant. */
    public static boolean isConfigured(Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED)
                && !AutofillAssistantStudy.getUrl().isEmpty()
                && AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn();
    }

    /** Starts Autofill Assistant on the given {@code activity}. */
    public static void start(CustomTabActivity activity) {
        Map<String, String> parameters = extractParameters(activity.getInitialIntent().getExtras());
        parameters.remove(PARAMETER_ENABLED);

        AutofillAssistantUiController controller =
                new AutofillAssistantUiController(activity, parameters);
        AutofillAssistantUiDelegate uiDelegate =
                new AutofillAssistantUiDelegate(activity, controller);
        UiDelegateHolder delegateHolder = new UiDelegateHolder(controller, uiDelegate);
        controller.setUiDelegateHolder(delegateHolder);
        initTabObservers(activity, delegateHolder);

        AutofillAssistantUiDelegate.Details initialDetails = makeDetailsFromParameters(parameters);
        controller.maybeUpdateDetails(initialDetails);

        uiDelegate.startOrSkipInitScreen();
    }

    private static void initTabObservers(
            CustomTabActivity activity, UiDelegateHolder delegateHolder) {
        // Shut down Autofill Assistant when the tab is detached from the activity.
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
    private static boolean getBooleanParameter(Bundle extras, String parameterName) {
        return extras.getBoolean(INTENT_EXTRA_PREFIX + parameterName, false);
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

    // TODO(crbug.com/806868): Create a fallback when there are no parameters for details.
    // TODO(crbug.com/806868): Create a fallback when there are no parameters for details.
    private static AutofillAssistantUiDelegate.Details makeDetailsFromParameters(
            Map<String, String> parameters) {
        String title = "";
        String description = "";
        Date date = null;
        for (String key : parameters.keySet()) {
            if (key.contains("E_NAME")) {
                title = parameters.get(key);
                continue;
            }

            if (key.contains("R_NAME")) {
                description = parameters.get(key);
                continue;
            }

            if (key.contains("DATETIME")) {
                try {
                    // The parameter contains the timezone shift from the current location, that we
                    // don't care about.
                    date = new SimpleDateFormat(RFC_3339_FORMAT_WITHOUT_TIMEZONE, Locale.ROOT)
                                   .parse(parameters.get(key));
                } catch (ParseException e) {
                    // Ignore.
                }
            }
        }

        return new AutofillAssistantUiDelegate.Details(
                title, /* url= */ "", date, description, /* isFinal= */ false);
    }
}
