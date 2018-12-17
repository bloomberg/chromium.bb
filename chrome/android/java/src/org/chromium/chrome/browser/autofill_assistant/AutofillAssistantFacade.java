// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.util.IntentUtils;

import java.util.HashMap;
import java.util.Map;

/** Facade for starting Autofill Assistant on a custom tab. */
public class AutofillAssistantFacade {
    /**
     * Prefix for Intent extras relevant to this feature.
     *
     * <p>Intent starting with this prefix are reported to the controller as parameters, except for
     * the ones starting with {@code INTENT_SPECIAL_PREFIX}.
     */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Prefix for intent extras which are not parameters. */
    private static final String INTENT_SPECIAL_PREFIX = INTENT_EXTRA_PREFIX + "special.";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /**
     * Boolean parameter that trusted apps can use to declare that the user has agreed to Terms and
     * Conditions that cover the use of Autofill Assistant in Chrome for that specific invocation.
     */
    private static final String AGREED_TO_TC = "AGREED_TO_TC";

    /** Pending intent sent by first-party apps. */
    private static final String PENDING_INTENT_NAME = INTENT_SPECIAL_PREFIX + "PENDING_INTENT";

    /** Package names of trusted first-party apps, from the pending intent. */
    private static final String[] TRUSTED_CALLER_PACKAGES = {
            "com.google.android.googlequicksearchbox", // GSA
    };

    /** Returns true if conditions are satisfied to attempt to start Autofill Assistant. */
    public static boolean isConfigured(@Nullable Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED);
    }

    /** Starts Autofill Assistant on the given {@code activity}. */
    public static void start(ChromeActivity activity) {
        startInternal(activity, /* controller= */ null);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void startInternal(
            ChromeActivity activity, @Nullable AbstractAutofillAssistantUiController controller) {
        Map<String, String> parameters = extractParameters(activity.getInitialIntent().getExtras());
        parameters.remove(PARAMETER_ENABLED);

        final AbstractAutofillAssistantUiController targetController = controller == null
                ? new AutofillAssistantUiController(activity, parameters)
                : controller;
        if (canStart(activity.getInitialIntent())) {
            initiateAutofillAssistant(activity, parameters, targetController);
            return;
        }
        if (AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            FirstRunScreen.show(activity, (result) -> {
                if (result) initiateAutofillAssistant(activity, parameters, targetController);
            });
            return;
        }

        // We don't have consent to start Autofill Assistant and cannot show initial screen.
        // Do nothing.
    }

    /**
     * Instantiates all essential Autofill Assistant components and starts it.
     */
    private static void initiateAutofillAssistant(ChromeActivity activity,
            Map<String, String> parameters, AbstractAutofillAssistantUiController controller) {
        UiDelegateHolder delegateHolder = new UiDelegateHolder(
                new AutofillAssistantUiDelegate(activity, controller), controller);
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
        return extras != null
                && IntentUtils.safeGetBoolean(extras, INTENT_EXTRA_PREFIX + parameterName, false);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(@Nullable Bundle extras) {
        Map<String, String> result = new HashMap<>();
        if (extras != null) {
            for (String key : extras.keySet()) {
                if (key.startsWith(INTENT_EXTRA_PREFIX) && !key.startsWith(INTENT_SPECIAL_PREFIX)) {
                    result.put(key.substring(INTENT_EXTRA_PREFIX.length()),
                            extras.get(key).toString());
                }
            }
        }
        return result;
    }

    /** Returns {@code true} if we can start right away. */
    private static boolean canStart(Intent intent) {
        return (AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn()
                       && !AutofillAssistantPreferencesUtil.getShowOnboarding())
                || hasAgreedToTc(intent);
    }

    /**
     * Returns {@code true} if the user has already agreed to specific terms and conditions for the
     * current task, that cover the use of autofill assistant. There's no need to show the generic
     * first-time screen for that call.
     */
    private static boolean hasAgreedToTc(Intent intent) {
        return getBooleanParameter(intent.getExtras(), AGREED_TO_TC)
                && callerIsOnWhitelist(intent, TRUSTED_CALLER_PACKAGES);
    }

    /** Returns {@code true} if the caller is on the given whitelist. */
    private static boolean callerIsOnWhitelist(Intent intent, String[] whitelist) {
        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelableExtra(intent, PENDING_INTENT_NAME);
        if (pendingIntent == null) {
            return false;
        }
        String packageName = ApiCompatibilityUtils.getCreatorPackage(pendingIntent);
        for (String whitelistedPackage : whitelist) {
            if (whitelistedPackage.equals(packageName)) {
                return true;
            }
        }
        return false;
    }
}
