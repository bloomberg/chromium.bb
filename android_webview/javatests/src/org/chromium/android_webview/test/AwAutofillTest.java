// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Parcel;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.ViewStructure.HtmlInfo;
import android.view.ViewStructure.HtmlInfo.Builder;
import android.view.autofill.AutofillId;
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
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for WebView Autofill.
 */
@RunWith(AwJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
@SuppressLint("NewApi")
public class AwAutofillTest {
    /**
     * This class only implements the necessary methods of ViewStructure for testing.
     */
    public static class TestViewStructure extends ViewStructure {
        /**
         * Implementation of HtmlInfo.
         */
        public static class AwHtmlInfo extends HtmlInfo {
            private String mTag;
            private List<Pair<String, String>> mAttributes;
            public AwHtmlInfo(String tag, List<Pair<String, String>> attributes) {
                mTag = tag;
                mAttributes = attributes;
            }

            @Override
            public List<Pair<String, String>> getAttributes() {
                return mAttributes;
            }

            public String getAttribute(String attribute) {
                for (Pair<String, String> pair : mAttributes) {
                    if (attribute.equals(pair.first)) {
                        return pair.second;
                    }
                }
                return null;
            }

            @Override
            public String getTag() {
                return mTag;
            }
        }

        /**
         * Implementation of Builder
         */
        public static class AwBuilder extends Builder {
            private String mTag;
            private ArrayList<Pair<String, String>> mAttributes;
            public AwBuilder(String tag) {
                mTag = tag;
                mAttributes = new ArrayList<Pair<String, String>>();
            }

            @Override
            public Builder addAttribute(String name, String value) {
                mAttributes.add(new Pair<String, String>(name, value));
                return this;
            }

            @Override
            public HtmlInfo build() {
                return new AwHtmlInfo(mTag, mAttributes);
            }
        }

        public TestViewStructure() {
            mChildren = new ArrayList<TestViewStructure>();
        }

        @Override
        public void setAlpha(float alpha) {}

        @Override
        public void setAccessibilityFocused(boolean state) {}

        @Override
        public void setCheckable(boolean state) {}

        @Override
        public void setChecked(boolean state) {
            mChecked = state;
        }

        public boolean getChecked() {
            return mChecked;
        }

        @Override
        public void setActivated(boolean state) {}

        @Override
        public CharSequence getText() {
            return null;
        }

        @Override
        public int getTextSelectionStart() {
            return 0;
        }

        @Override
        public int getTextSelectionEnd() {
            return 0;
        }

        @Override
        public CharSequence getHint() {
            return mHint;
        }

        @Override
        public Bundle getExtras() {
            return null;
        }

        @Override
        public boolean hasExtras() {
            return false;
        }

        @Override
        public void setChildCount(int num) {}

        @Override
        public int addChildCount(int num) {
            int index = mChildCount;
            mChildCount += num;
            mChildren.ensureCapacity(mChildCount);
            return index;
        }

        @Override
        public int getChildCount() {
            return mChildren.size();
        }

        @Override
        public ViewStructure newChild(int index) {
            TestViewStructure viewStructure = new TestViewStructure();
            if (index < mChildren.size()) {
                mChildren.set(index, viewStructure);
            } else if (index == mChildren.size()) {
                mChildren.add(viewStructure);
            } else {
                // Failed intentionally, we shouldn't run into this case.
                mChildren.add(index, viewStructure);
            }
            return viewStructure;
        }

        public TestViewStructure getChild(int index) {
            return mChildren.get(index);
        }

        @Override
        public ViewStructure asyncNewChild(int index) {
            return null;
        }

        @Override
        public void asyncCommit() {}

        @Override
        public AutofillId getAutofillId() {
            Parcel parcel = Parcel.obtain();
            // Check AutofillId(Parcel) in AutofillId.java, always set parent id as 0.
            parcel.writeInt(0);
            parcel.writeInt(1);
            parcel.writeInt(mId);

            return AutofillId.CREATOR.createFromParcel(parcel);
        }

        @Override
        public Builder newHtmlInfoBuilder(String tag) {
            return new AwBuilder(tag);
        }

        @Override
        public void setAutofillHints(String[] arg0) {
            mAutofillHints = arg0.clone();
        }

        public String[] getAutofillHints() {
            if (mAutofillHints == null) return null;
            return mAutofillHints.clone();
        }

        @Override
        public void setAutofillId(AutofillId arg0) {}

        @Override
        public void setAutofillId(AutofillId arg0, int arg1) {
            mId = arg1;
        }

        public int getId() {
            return mId;
        }

        @Override
        public void setAutofillOptions(CharSequence[] arg0) {
            mAutofillOptions = arg0.clone();
        }

        public CharSequence[] getAutofillOptions() {
            if (mAutofillOptions == null) return null;
            return mAutofillOptions.clone();
        }

        @Override
        public void setAutofillType(int arg0) {
            mAutofillType = arg0;
        }

        public int getAutofillType() {
            return mAutofillType;
        }

        @Override
        public void setAutofillValue(AutofillValue arg0) {
            mAutofillValue = arg0;
        }

        public AutofillValue getAutofillValue() {
            return mAutofillValue;
        }

        @Override
        public void setId(int id, String packageName, String typeName, String entryName) {}

        @Override
        public void setDimens(int left, int top, int scrollX, int scrollY, int width, int height) {}

        @Override
        public void setElevation(float elevation) {}

        @Override
        public void setEnabled(boolean state) {}

        @Override
        public void setClickable(boolean state) {}

        @Override
        public void setLongClickable(boolean state) {}

        @Override
        public void setContextClickable(boolean state) {}

        @Override
        public void setFocusable(boolean state) {}

        @Override
        public void setFocused(boolean state) {}

        @Override
        public void setClassName(String className) {
            mClassName = className;
        }

        public String getClassName() {
            return mClassName;
        }

        @Override
        public void setContentDescription(CharSequence contentDescription) {}

        @Override
        public void setDataIsSensitive(boolean arg0) {
            mDataIsSensitive = arg0;
        }

        public boolean getDataIsSensitive() {
            return mDataIsSensitive;
        }

        @Override
        public void setHint(CharSequence hint) {
            mHint = hint;
        }

        @Override
        public void setHtmlInfo(HtmlInfo arg0) {
            mAwHtmlInfo = (AwHtmlInfo) arg0;
        }

        public AwHtmlInfo getHtmlInfo() {
            return mAwHtmlInfo;
        }

        @Override
        public void setInputType(int arg0) {}

        @Override
        public void setLocaleList(LocaleList arg0) {}

        @Override
        public void setOpaque(boolean arg0) {}

        @Override
        public void setTransformation(Matrix matrix) {}

        @Override
        public void setVisibility(int visibility) {}

        @Override
        public void setSelected(boolean state) {}

        @Override
        public void setText(CharSequence text) {}

        @Override
        public void setText(CharSequence text, int selectionStart, int selectionEnd) {}

        @Override
        public void setTextStyle(float size, int fgColor, int bgColor, int style) {}

        @Override
        public void setTextLines(int[] charOffsets, int[] baselines) {}

        @Override
        public void setWebDomain(String webDomain) {
            mWebDomain = webDomain;
        }

        public String getWebDomain() {
            return mWebDomain;
        }

        private int mAutofillType;
        private CharSequence mHint;
        private String[] mAutofillHints;
        private int mId;
        private String mClassName;
        private String mWebDomain;
        private int mChildCount;
        private ArrayList<TestViewStructure> mChildren;
        private CharSequence[] mAutofillOptions;
        private AutofillValue mAutofillValue;
        private boolean mDataIsSensitive;
        private AwHtmlInfo mAwHtmlInfo;
        private boolean mChecked;
    }

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
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
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

            // Verify form filled correctly in ViewStructure.
            URL pageURL = new URL(url);
            String webDomain =
                    new URL(pageURL.getProtocol(), pageURL.getHost(), pageURL.getPort(), "/")
                            .toString();
            assertEquals(webDomain, viewStructure.getWebDomain());
            // WebView shouldn't set class name.
            assertNull(viewStructure.getClassName());
            TestViewStructure.AwHtmlInfo htmlInfoForm = viewStructure.getHtmlInfo();
            assertEquals("form", htmlInfoForm.getTag());
            assertEquals("formname", htmlInfoForm.getAttribute("name"));

            // Verify input text control filled correctly in ViewStructure.
            TestViewStructure child0 = viewStructure.getChild(0);
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
