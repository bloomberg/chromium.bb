// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

/**
 * Abstract AutofillAssistantUiController, this is created for testing UI.
 *
 * TODO(crbug.com/806868): Refactor this class to AutofillAssistantUiController interface and the
 * original AutofillAssistantUiController to AutofillAssistantUiControllerImpl after done necessary
 * merges to M72.
 */
abstract class AbstractAutofillAssistantUiController
        implements AutofillAssistantUiDelegate.Client, UiDelegateHolder.Listener {}
