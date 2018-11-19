// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiDelegate.Client;

/**
 * Automatically extracts context information and serializes it in JSON form.
 */
class FeedbackContext extends JSONObject {
    static String buildContextString(ChromeActivity activity, Client client, Details details,
            String statusMessage, int indentSpaces) {
        try {
            return new FeedbackContext(activity, client, details, statusMessage)
                    .toString(indentSpaces);
        } catch (JSONException e) {
            // Note: it is potentially unsafe to return e.getMessage(): the exception message
            // could be wrangled and used as an attack vector when arriving at the JSON parser.
            return "{\"error\": \"Failed to convert feedback context to string.\"}";
        }
    }

    private FeedbackContext(ChromeActivity activity, Client client, Details details,
            String statusMessage) throws JSONException {
        addActivityInformation(activity);
        addClientContext(client);
        put("movie", details.toJSONObject());
        put("status", statusMessage);
    }

    private void addActivityInformation(ChromeActivity activity) throws JSONException {
        put("intent-action", activity.getInitialIntent().getAction());
        put("intent-data", activity.getInitialIntent().getDataString());
    }

    private void addClientContext(Client client) throws JSONException {
        // Try to parse the debug context as JSON object. If that fails, just add the string as-is.
        try {
            put("debug-context", new JSONObject(client.getDebugContext()));
        } catch (JSONException encodingException) {
            put("debug-context", client.getDebugContext());
        }
    }
}
