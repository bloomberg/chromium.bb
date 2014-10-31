// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.tests;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.components.devtools_bridge.apiary.ApiaryClientFactory;
import org.chromium.components.devtools_bridge.apiary.TestApiaryClientFactory;
import org.chromium.components.devtools_bridge.ui.GCDRegistrationFragment;
import org.chromium.components.devtools_bridge.ui.RemoteInstanceListFragment;

/**
 * Activity for testing devtools bridge.
 */
public class DebugActivity extends Activity {
    private static final int LAYOUT_ID = 1000;

    private LinearLayout mLayout;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mLayout = new LinearLayout(this);
        mLayout.setId(LAYOUT_ID);
        mLayout.setOrientation(LinearLayout.VERTICAL);

        String intro =
                "To test LocalTunnelBridge manually: \n"
                + "1. Enable USB debugging.\n"
                + "2. Run ChromeShell along with this app.\n"
                + "3. Start the LocalTunnelBridge.\n"
                + "4. Connect the device to a desktop via USB.\n"
                + "5. Open chrome://inspect#devices on desktop Chrome.\n"
                + "6. Observe 2 identical Chrome Shells on the device.";

        TextView textView = new TextView(this);
        textView.setText(intro);
        mLayout.addView(textView);

        addActionButton("Start LocalTunnelBridge", DebugService.START_TUNNEL_BRIDGE_ACTION);
        addActionButton("Start LocalSessionBridge", DebugService.START_SESSION_BRIDGE_ACTION);
        addActionButton("Start hosted DevToolsBridgeServer", DebugService.START_SERVER_ACTION);
        addActionButton("Stop", DebugService.STOP_ACTION);

        LayoutParams layoutParam = new LayoutParams(LayoutParams.MATCH_PARENT,
                                                    LayoutParams.MATCH_PARENT);

        getFragmentManager()
                .beginTransaction()
                .add(LAYOUT_ID, new TestGCDRegistrationFragment())
                .add(LAYOUT_ID, new RemoteInstanceListFragment())
                .commit();

        setContentView(mLayout, layoutParam);
    }

    private void addActionButton(String text, String action) {
        Button button = new Button(this);
        button.setText(text);
        button.setOnClickListener(new SendActionOnClickListener(action));
        mLayout.addView(button);
    }

    private class SendActionOnClickListener implements View.OnClickListener {
        private final String mAction;

        SendActionOnClickListener(String action) {
            mAction = action;
        }

        @Override
        public void onClick(View v) {
            Intent intent = new Intent(DebugActivity.this, DebugService.class);
            intent.setAction(mAction);
            startService(intent);
        }
    }

    private static class TestGCDRegistrationFragment
            extends GCDRegistrationFragment implements View.OnClickListener {
        private Button mButton;

        @Override
        public View onCreateView(
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            mButton = new Button(getActivity());
            mButton.setOnClickListener(this);
            updateText();
            return mButton;
        }

        public void updateText() {
            mButton.setText(isRegistered()
                    ? "Unregister (registered for " + getOwner() + ")"
                    : "Register in GCD");
        }

        @Override
        protected void onRegistrationStatusChange() {
            updateText();
        }

        @Override
        protected ApiaryClientFactory newClientFactory() {
            return new TestApiaryClientFactory();
        }

        @Override
        protected String generateDisplayName() {
            return Build.MODEL + " Test app";
        }

        @Override
        public void onClick(View v) {
            if (!isRegistered()) {
                register();
            } else {
                unregister();
            }
        }
    }
}

