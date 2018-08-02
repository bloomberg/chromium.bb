// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetTrigger.MANUAL_OPEN;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;

/**
 * This class provides helpers to record metrics related to the keyboard accessory and its sheets.
 * It can set up observers to observe {@link KeyboardAccessoryModel}s, {@link AccessorySheetModel}s
 * or {@link ListObservable<Item>}s changes and records metrics accordingly.
 */
class KeyboardAccessoryMetricsRecorder {
    static final String UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION =
            "KeyboardAccessory.AccessoryActionImpression";
    static final String UMA_KEYBOARD_ACCESSORY_ACTION_SELECTED =
            "KeyboardAccessory.AccessoryActionSelected";
    static final String UMA_KEYBOARD_ACCESSORY_BAR_SHOWN = "KeyboardAccessory.AccessoryBarShown";
    static final String UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS =
            "KeyboardAccessory.AccessorySheetSuggestionCount";
    static final String UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTION_SELECTED =
            "KeyboardAccessory.AccessorySheetSuggestionsSelected";
    static final String UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED =
            "KeyboardAccessory.AccessorySheetTriggered";
    static final String UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_PASSWORDS = "Passwords";

    /**
     * The Recorder itself should be stateless and have no need for an instance.
     */
    private KeyboardAccessoryMetricsRecorder() {}

    static void recordModelChanges(KeyboardAccessoryModel keyboardAccessoryModel) {
        keyboardAccessoryModel.addObserver((source, propertyKey) -> {
            if (propertyKey == KeyboardAccessoryModel.PropertyKey.VISIBLE) {
                if (keyboardAccessoryModel.isVisible()) {
                    recordAccessoryBarImpression(keyboardAccessoryModel);
                    recordAccessoryActionImpressions(keyboardAccessoryModel);
                }
                return;
            }
            if (propertyKey == KeyboardAccessoryModel.PropertyKey.ACTIVE_TAB
                    || propertyKey == KeyboardAccessoryModel.PropertyKey.TAB_SELECTION_CALLBACKS
                    || propertyKey == KeyboardAccessoryModel.PropertyKey.SUGGESTIONS) {
                return;
            }
            assert false : "Every property update needs to be handled explicitly!";
        });
    }

    static void recordModelChanges(AccessorySheetModel accessorySheetModel) {
        accessorySheetModel.addObserver((source, propertyKey) -> {
            if (propertyKey == AccessorySheetModel.PropertyKey.VISIBLE) {
                if (accessorySheetModel.isVisible()) {
                    int activeTab = accessorySheetModel.getActiveTabIndex();
                    if (activeTab >= 0 && activeTab < accessorySheetModel.getTabList().size()) {
                        recordSheetTrigger(
                                accessorySheetModel.getTabList().get(activeTab).getRecordingType(),
                                MANUAL_OPEN);
                    }
                } else {
                    recordSheetTrigger(AccessoryTabType.ALL, AccessorySheetTrigger.ANY_CLOSE);
                }
                return;
            }
            if (propertyKey == AccessorySheetModel.PropertyKey.ACTIVE_TAB_INDEX) {
                return;
            }
            assert false : "Every property update needs to be handled explicitly!";
        });
    }

    @VisibleForTesting
    static String getHistogramForType(String baseHistogram, @AccessoryTabType int tabType) {
        switch (tabType) {
            case AccessoryTabType.ALL:
                return baseHistogram;
            case AccessoryTabType.PASSWORDS:
                return baseHistogram + "." + UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_PASSWORDS;
        }
        assert false : "Undefined histogram for tab type " + tabType + " !";
        return "";
    }

    static void recordSheetTrigger(
            @AccessoryTabType int tabType, @AccessorySheetTrigger int bucket) {
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramForType(UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED, tabType), bucket,
                AccessorySheetTrigger.COUNT);
        if (tabType != AccessoryTabType.ALL) { // Record count for all tab types exactly once!
            RecordHistogram.recordEnumeratedHistogram(
                    getHistogramForType(
                            UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED, AccessoryTabType.ALL),
                    bucket, AccessorySheetTrigger.COUNT);
        }
    }

    static void recordActionImpression(@AccessoryAction int bucket) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION, bucket, AccessoryAction.COUNT);
    }

    static void recordActionSelected(@AccessoryAction int bucket) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_KEYBOARD_ACCESSORY_ACTION_SELECTED, bucket, AccessoryAction.COUNT);
    }

    static void recordSuggestionSelected(@AccessorySuggestionType int bucket) {
        RecordHistogram.recordEnumeratedHistogram(UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTION_SELECTED,
                bucket, AccessorySuggestionType.COUNT);
    }

    static void recordSheetSuggestions(
            @AccessoryTabType int tabType, SimpleListObservable<Item> suggestionList) {
        int interactiveSuggestions = 0;
        for (int i = 0; i < suggestionList.size(); ++i) {
            if (suggestionList.get(i).getType() == ItemType.SUGGESTION) ++interactiveSuggestions;
        }
        RecordHistogram.recordCount100Histogram(
                getHistogramForType(UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS, tabType),
                interactiveSuggestions);
        if (tabType != AccessoryTabType.ALL) { // Record count for all tab types exactly once!
            RecordHistogram.recordCount100Histogram(
                    getHistogramForType(
                            UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS, AccessoryTabType.ALL),
                    interactiveSuggestions);
        }
    }

    private static void recordAccessoryActionImpressions(
            KeyboardAccessoryModel keyboardAccessoryModel) {
        for (KeyboardAccessoryData.Action action : keyboardAccessoryModel.getActionList()) {
            recordActionImpression(action.getActionType());
        }
    }

    private static void recordAccessoryBarImpression(
            KeyboardAccessoryModel keyboardAccessoryModel) {
        boolean barImpressionRecorded = false;
        for (@AccessoryBarContents int bucket = 0; bucket < AccessoryBarContents.COUNT; ++bucket) {
            if (shouldRecordAccessoryImpression(bucket, keyboardAccessoryModel)) {
                RecordHistogram.recordEnumeratedHistogram(
                        UMA_KEYBOARD_ACCESSORY_BAR_SHOWN, bucket, AccessoryBarContents.COUNT);
                barImpressionRecorded = true;
            }
        }
        RecordHistogram.recordEnumeratedHistogram(UMA_KEYBOARD_ACCESSORY_BAR_SHOWN,
                barImpressionRecorded ? AccessoryBarContents.ANY_CONTENTS
                                      : AccessoryBarContents.NO_CONTENTS,
                AccessoryBarContents.COUNT);
    }

    private static boolean shouldRecordAccessoryImpression(
            int bucket, KeyboardAccessoryModel keyboardAccessoryModel) {
        switch (bucket) {
            case AccessoryBarContents.WITH_TABS:
                return keyboardAccessoryModel.getTabList().size() > 0;
            case AccessoryBarContents.WITH_ACTIONS:
                return keyboardAccessoryModel.getActionList().size() > 0;
            case AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS:
                return keyboardAccessoryModel.getAutofillSuggestions() != null;
            case AccessoryBarContents.ANY_CONTENTS: // Intentional fallthrough.
            case AccessoryBarContents.NO_CONTENTS:
                return false; // General impression is logged last.
        }
        assert false : "Did not check whether to record an impression bucket " + bucket + ".";
        return false;
    }
}
