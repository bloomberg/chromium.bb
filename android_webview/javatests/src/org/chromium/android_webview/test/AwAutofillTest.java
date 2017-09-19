// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.autofill.AutofillValue;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwAutofillManager;
import org.chromium.android_webview.AwAutofillProvider;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwTestBase.TestDependencyFactory;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.net.test.util.TestWebServer;

import java.net.URL;
import java.util.concurrent.Callable;

/**
 * Tests for WebView Autofill.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
@SuppressLint("NewApi")
public class AwAutofillTest {
    private static class AwAutofillManagerHelper extends AwAutofillManager {
        private CallbackHelper mVirtualViewEntered = new CallbackHelper();
        private CallbackHelper mVirtualValueChanged = new CallbackHelper();
        private AwContents mAwContents;
        private TestViewStructure mTestViewStructure;
        private SparseArray<AutofillValue> mAutofillValues;

        public AwAutofillManagerHelper(Context context) {
            super(context);
        }

        @Override
        public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
            mVirtualViewEntered.notifyCalled();
        }

        public void waitForNotifyVirtualViewEnteredCalled() throws Throwable {
            int count = mVirtualViewEntered.getCallCount();
            mVirtualViewEntered.waitForCallback(count);
        }

        public SparseArray<AutofillValue> waitForNotifyVirtualValueChanged(int times)
                throws Throwable {
            int count = mVirtualValueChanged.getCallCount();
            mVirtualValueChanged.waitForCallback(count, times);
            return mAutofillValues;
        }

        public void setAwContents(AwContents awContents) {
            mAwContents = awContents;
        }

        public void invokeOnProvideAutoFillVirtualStructure() {
            mTestViewStructure = new TestViewStructure();
            mAwContents.onProvideAutoFillVirtualStructure(mTestViewStructure, 1);
        }

        public void invokeAutofill(SparseArray<AutofillValue> values) {
            mAutofillValues = new SparseArray<AutofillValue>();
            mAwContents.autofill(values);
        }

        public TestViewStructure getTestViewStructure() {
            return mTestViewStructure;
        }

        @Override
        public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
            if (mAutofillValues != null) mAutofillValues.append(childId, value);
            mVirtualValueChanged.notifyCalled();
        }
    }

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwAutofillManagerHelper mHelper;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(
                mContentsClient, false, new TestDependencyFactory() {
                    @Override
                    public AutofillProvider createAutofillProvider(
                            Context context, ViewGroup containerView) {
                        mHelper = new AwAutofillManagerHelper(context);
                        return new AwAutofillProvider(containerView, mHelper);
                    }
                });
        mHelper.setAwContents(mTestContainerView.getAwContents());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBasicAutofill() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        mActivityTestRule.enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
        final String data = "<html><head></head><body><form action='a.html'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='checkbox' id='checkbox1' name='showpassword'>"
                + "<select id='select1' name='month'>"
                + "<option value='1'>Jan</option>"
                + "<option value='2'>Feb</option>"
                + "</select>"
                + "<input type='submit'>"
                + "</form></body></html>";
        final int totalControls = 3;
        final String file = "/login.html";
        try {
            final String url = webServer.setResponse(file, data, null);
            mActivityTestRule.loadUrlSync(mTestContainerView.getAwContents(),
                    mContentsClient.getOnPageFinishedHelper(), url);
            mActivityTestRule.executeJavaScriptAndWaitForResult(mTestContainerView.getAwContents(),
                    mContentsClient, "document.getElementById('text1').select();");
            dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            mHelper.waitForNotifyVirtualViewEnteredCalled();
            mHelper.invokeOnProvideAutoFillVirtualStructure();
            TestViewStructure viewStructure = mHelper.getTestViewStructure();
            assertNotNull(viewStructure);
            assertEquals(totalControls, viewStructure.getChildCount());
            URL pageURL = new URL(url);
            String webDomain =
                    new URL(pageURL.getProtocol(), pageURL.getHost(), pageURL.getPort(), "/")
                            .toString();
            assertEquals(webDomain, viewStructure.getWebDomain());
            TestViewStructure child0 = viewStructure.getChild(0);

            // Verify input text control filled correctly in ViewStructure.
            assertEquals(View.AUTOFILL_TYPE_TEXT, child0.getAutofillType());
            assertEquals("placeholder@placeholder.com", child0.getHint());
            assertEquals("username", child0.getAutofillHints()[0]);
            assertEquals("name", child0.getAutofillHints()[1]);
            TestViewStructure.AwHtmlInfo htmlInfo0 = child0.getHtmlInfo();
            assertEquals("text", htmlInfo0.getAttribute("type"));
            assertEquals("username", htmlInfo0.getAttribute("name"));

            // Verify checkbox control filled correctly in ViewStructure.
            TestViewStructure child1 = viewStructure.getChild(1);
            assertEquals(View.AUTOFILL_TYPE_TOGGLE, child1.getAutofillType());
            assertEquals("", child1.getHint());
            assertNull(child1.getAutofillHints());
            TestViewStructure.AwHtmlInfo htmlInfo1 = child1.getHtmlInfo();
            assertEquals("checkbox", htmlInfo1.getAttribute("type"));
            assertEquals("showpassword", htmlInfo1.getAttribute("name"));

            // Verify select control filled correctly in ViewStructure.
            TestViewStructure child2 = viewStructure.getChild(2);
            assertEquals(View.AUTOFILL_TYPE_LIST, child2.getAutofillType());
            assertEquals("", child2.getHint());
            assertNull(child2.getAutofillHints());
            TestViewStructure.AwHtmlInfo htmlInfo2 = child2.getHtmlInfo();
            assertEquals("month", htmlInfo2.getAttribute("name"));
            CharSequence[] options = child2.getAutofillOptions();
            assertEquals("Jan", options[0]);
            assertEquals("Feb", options[1]);

            // Autofill form and verify filled values.
            SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
            values.append(child0.getId(), AutofillValue.forText("example@example.com"));
            values.append(child1.getId(), AutofillValue.forToggle(true));
            values.append(child2.getId(), AutofillValue.forList(1));
            mHelper.invokeAutofill(values);
            mHelper.waitForNotifyVirtualValueChanged(totalControls);

            // Verify form filled by Javascript
            String value0 = mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mTestContainerView.getAwContents(), mContentsClient,
                    "document.getElementById('text1').value;");
            assertEquals("\"example@example.com\"", value0);
            String value1 = mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mTestContainerView.getAwContents(), mContentsClient,
                    "document.getElementById('checkbox1').value;");
            assertEquals("\"on\"", value1);
            String value2 = mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mTestContainerView.getAwContents(), mContentsClient,
                    "document.getElementById('select1').value;");
            assertEquals("\"2\"", value2);
        } finally {
            webServer.shutdown();
        }
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, code));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, code));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return mActivityTestRule.runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mTestContainerView.dispatchKeyEvent(event);
            }
        });
    }
}
