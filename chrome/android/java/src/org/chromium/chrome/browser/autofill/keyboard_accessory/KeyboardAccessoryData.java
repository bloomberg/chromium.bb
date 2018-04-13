// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

/**
 * Interfaces in this class are used to pass data into keyboard accessory component.
 */
public class KeyboardAccessoryData {
    /**
     * A provider notifies all registered {@link ActionListObserver} if the list of actions
     * changes.
     */
    public interface ActionListProvider {
        /**
         * Every observer added by this need to be notified whenever the list of action changes
         * @param observer The observer to be notified.
         */
        void addObserver(ActionListObserver observer);
    }

    /**
     * An observer receives notifications from an {@link ActionListProvider} it is subscribed to.
     */
    public interface ActionListObserver {
        /**
         * A provider calls this function with a list of actions that should be available in the
         * keyboard accessory.
         * @param actions The actions to be displayed in the Accessory. It's a native array as the
         *                provider is typically a bridge called via JNI which prefers native types.
         */
        void onActionsAvailable(Action[] actions);
    }

    /**
     * Describes a tab which should be displayed as a small icon at the start of the keyboard
     * accessory. Typically, a tab is responsible to change the bottom sheet below the accessory.
     */
    public interface Tab {
        // E.g. getIcon(), getDescription() and onTabSelected()
    }

    /**
     * This describes an action that can be invoked from the keyboard accessory.
     * The most prominent example hereof is the "Generate Password" action.
     */
    public interface Action {
        // E.g. getCaption() or onActionSelected();
    }

    private KeyboardAccessoryData() {}
}
