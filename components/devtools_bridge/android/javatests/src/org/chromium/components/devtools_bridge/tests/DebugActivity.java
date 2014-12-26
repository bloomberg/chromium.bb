// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.tests;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.components.devtools_bridge.ui.GCDRegistrationFragment;
import org.chromium.components.devtools_bridge.ui.RemoteInstanceListFragment;

/**
 * Activity for testing devtools bridge.
 */
public class DebugActivity extends Activity {
    private static final int LAYOUT_ID = 1000;
    private static final String DOC_HREF =
            "https://docs.google.com/a/chromium.org/document/d/"
            + "188V6TWV8ivbjAPIp6aqwffWrY78xN2an5REajZHpunk/edit?usp=sharing";

    private LinearLayout mLayout;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mLayout = new LinearLayout(this);
        mLayout.setId(LAYOUT_ID);
        mLayout.setOrientation(LinearLayout.VERTICAL);

        TextView textView = new TextView(this);
        textView.setText(Html.fromHtml(
                "See <a href=\"" + DOC_HREF + "\">testing instructions</a>."));
        textView.setMovementMethod(LinkMovementMethod.getInstance());
        mLayout.addView(textView);

        addActionButton("Start LocalSessionBridge", DebugService.START_SESSION_BRIDGE_ACTION);
        addActionButton("Start DevToolsBridgeServer", DebugService.START_SERVER_ACTION);
        addActionButton("Stop", DebugService.STOP_ACTION);

        LayoutParams layoutParam = new LayoutParams(LayoutParams.MATCH_PARENT,
                                                    LayoutParams.MATCH_PARENT);

        getFragmentManager()
                .beginTransaction()
                .add(LAYOUT_ID, new TestGCDRegistrationFragment())
                .add(LAYOUT_ID, new RemoteInstanceListFragmentImpl())
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

    private final class RemoteInstanceListFragmentImpl extends RemoteInstanceListFragment {
        @Override
        protected void connect(String oAuthToken, String remoteInstanceId) {
            startService(new Intent(DebugActivity.this, DebugService.class)
                    .setAction(DebugService.START_GCD_CLIENT_ACTION)
                    .putExtra(DebugService.EXTRA_OAUTH_TOKEN, oAuthToken)
                    .putExtra(DebugService.EXTRA_REMOTE_INSTANCE_ID, remoteInstanceId));
        }
    }
}

