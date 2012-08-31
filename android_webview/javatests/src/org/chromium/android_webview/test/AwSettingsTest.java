// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.Feature;
import org.chromium.base.test.UrlUtils;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentViewCore;

import java.util.concurrent.Callable;

/**
 * A test suite for ContentSettings class. The key objective is to verify that each
 * settings applies either to each individual view or to all views of the
 * application.
 */
public class AwSettingsTest extends AndroidWebViewTestBase {
    private static final boolean ENABLED = true;
    private static final boolean DISABLED = false;

    /**
     * A helper class for testing a particular preference from ContentSettings.
     * The generic type T is the type of the setting. Usually, to test an
     * effect of the preference, JS code is executed that sets document's title.
     * In this case, requiresJsEnabled constructor argument must be set to true.
     */
    abstract class AwSettingsTestHelper<T> {
        protected final ContentViewCore mContentViewCore;
        protected final TestAwContentsClient mContentViewClient;
        protected final ContentSettings mContentSettings;

        AwSettingsTestHelper(ContentViewCore contentViewCore,
                             TestAwContentsClient contentViewClient,
                             boolean requiresJsEnabled) throws Throwable {
            mContentViewCore = contentViewCore;
            mContentViewClient = contentViewClient;
            mContentSettings = getContentSettingsOnUiThread(mContentViewCore);
            if (requiresJsEnabled) {
                mContentSettings.setJavaScriptEnabled(true);
            }
        }

        void ensureSettingHasAlteredValue() throws Throwable {
            ensureSettingHasValue(getAlteredValue());
        }

        void ensureSettingHasInitialValue() throws Throwable {
            ensureSettingHasValue(getInitialValue());
        }

        void setAlteredSettingValue() throws Throwable {
            setCurrentValue(getAlteredValue());
        }

        void setInitialSettingValue() throws Throwable {
            setCurrentValue(getInitialValue());
        }

        protected abstract T getAlteredValue();

        protected abstract T getInitialValue();

        protected abstract T getCurrentValue();

        protected abstract void setCurrentValue(T value);

        protected abstract void doEnsureSettingHasValue(T value) throws Throwable;

        protected String getTitleOnUiThread() throws Throwable {
            return AwSettingsTest.this.getTitleOnUiThread(mContentViewCore);
        }

        protected void loadDataSync(String data) throws Throwable {
            AwSettingsTest.this.loadDataSync(
                mContentViewCore,
                mContentViewClient.getOnPageFinishedHelper(),
                data,
                "text/html",
                false);
        }

        private void ensureSettingHasValue(T value) throws Throwable {
            assertEquals(value, getCurrentValue());
            doEnsureSettingHasValue(value);
        }
    }

    class AwSettingsJavaScriptTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String JS_ENABLED_STRING = "JS Enabled";
        private static final String JS_DISABLED_STRING = "JS Disabled";

        AwSettingsJavaScriptTestHelper(ContentViewCore contentViewCore,
                                       TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mContentSettings.getJavaScriptEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setJavaScriptEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            assertEquals(
                value == ENABLED ? JS_ENABLED_STRING : JS_DISABLED_STRING,
                getTitleOnUiThread());
        }

        private String getData() {
            return "<html><head><title>" + JS_DISABLED_STRING + "</title>"
                    + "</head><body onload=\"document.title='" + JS_ENABLED_STRING
                    + "';\"></body></html>";
        }
    }

    class AwSettingsPluginsTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String PLUGINS_ENABLED_STRING = "Embed";
        private static final String PLUGINS_DISABLED_STRING = "NoEmbed";

        AwSettingsPluginsTestHelper(ContentViewCore contentViewCore,
                                    TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mContentSettings.getPluginsEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setPluginsEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            assertEquals(
                value == ENABLED ? PLUGINS_ENABLED_STRING : PLUGINS_DISABLED_STRING,
                getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = document.body.innerText;\">"
                    + "<noembed>No</noembed><span>Embed</span></body></html>";
        }
    }

    class AwSettingsStandardFontFamilyTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsStandardFontFamilyTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
        }

        @Override
        protected String getAlteredValue() {
            return "cursive";
        }

        @Override
        protected String getInitialValue() {
            return "sans-serif";
        }

        @Override
        protected String getCurrentValue() {
            return mContentSettings.getStandardFontFamily();
        }

        @Override
        protected void setCurrentValue(String value) {
            mContentSettings.setStandardFontFamily(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            assertEquals(value, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = " +
                    "getComputedStyle(document.body).getPropertyValue('font-family');\">"
                    + "</body></html>";
        }
    }

    class AwSettingsDefaultFontSizeTestHelper extends AwSettingsTestHelper<Integer> {
        AwSettingsDefaultFontSizeTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
        }

        @Override
        protected Integer getAlteredValue() {
            return 42;
        }

        @Override
        protected Integer getInitialValue() {
            return 16;
        }

        @Override
        protected Integer getCurrentValue() {
            return mContentSettings.getDefaultFontSize();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mContentSettings.setDefaultFontSize(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            loadDataSync(getData());
            assertEquals(value.toString() + "px", getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = " +
                    "getComputedStyle(document.body).getPropertyValue('font-size');\">"
                    + "</body></html>";
        }
    }

    class AwSettingsDefaultTextEncodingTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsDefaultTextEncodingTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
        }

        @Override
        protected String getAlteredValue() {
            return "utf-8";
        }

        @Override
        protected String getInitialValue() {
            return "Latin-1";
        }

        @Override
        protected String getCurrentValue() {
            return mContentSettings.getDefaultTextEncodingName();
        }

        @Override
        protected void setCurrentValue(String value) {
            mContentSettings.setDefaultTextEncodingName(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            assertEquals(value, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload='document.title=document.defaultCharset'></body></html>";
        }
    }

    class AwSettingsUserAgentTestHelper extends AwSettingsTestHelper<String> {
        private final String mDefaultUa;
        private static final String DEFAULT_UA = "";
        private static final String CUSTOM_UA = "ChromeViewTest";

        AwSettingsUserAgentTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
            mDefaultUa = mContentSettings.getUserAgentString();
        }

        @Override
        protected String getAlteredValue() {
            return CUSTOM_UA;
        }

        @Override
        protected String getInitialValue() {
            return DEFAULT_UA;
        }

        @Override
        protected String getCurrentValue() {
            // The test framework expects that getXXX() == Z after setXXX(Z).
            // But setUserAgentString("" / null) resets the UA string to default,
            // and getUserAgentString returns the default UA string afterwards.
            // To align with the framework, we return an empty string instead of
            // the default UA.
            String currentUa = mContentSettings.getUserAgentString();
            return mDefaultUa.equals(currentUa) ? DEFAULT_UA : currentUa;
        }

        @Override
        protected void setCurrentValue(String value) {
            mContentSettings.setUserAgentString(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            assertEquals(
                value == DEFAULT_UA ? mDefaultUa : value,
                getTitleOnUiThread());
        }

        private String getData() {
            return "<html>" +
                    "<body onload='document.writeln(document.title=navigator.userAgent)'></body>" +
                    "</html>";
        }
    }

    class AwSettingsDomStorageEnabledTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String NO_LOCAL_STORAGE = "No localStorage";
        private static final String HAS_LOCAL_STORAGE = "Has localStorage";

        AwSettingsDomStorageEnabledTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mContentSettings.getDomStorageEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setDomStorageEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It is not permitted to access localStorage from data URLs in WebKit,
            // that is why a standalone page must be used.
            AwSettingsTest.this.loadUrlSync(
                mContentViewCore,
                mContentViewClient.getOnPageFinishedHelper(),
                UrlUtils.getTestFileUrl("webview/localStorage.html"));
            assertEquals(
                value == ENABLED ? HAS_LOCAL_STORAGE : NO_LOCAL_STORAGE,
                getTitleOnUiThread());
        }
    }

    // The test verifies that JavaScript is disabled upon WebView
    // creation without accessing ContentSettings. If the test passes,
    // it means that WebView-specific web preferences configuration
    // is applied on WebView creation. JS state is used, because it is
    // enabled by default in Chrome, but must be disabled by default
    // in WebView.
    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testJavaScriptDisabledByDefault() throws Throwable {
        final String JS_ENABLED_STRING = "JS has run";
        final String JS_DISABLED_STRING = "JS has not run";
        final String TEST_PAGE_HTML =
                "<html><head><title>" + JS_DISABLED_STRING + "</title>"
                + "</head><body onload=\"document.title='" + JS_ENABLED_STRING
                + "';\"></body></html>";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        loadDataSync(
            contentView,
            contentClient.getOnPageFinishedHelper(),
            TEST_PAGE_HTML,
            "text/html",
            false);
        assertEquals(JS_DISABLED_STRING, getTitleOnUiThread(contentView));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testJavaScriptEnabledNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsJavaScriptTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsJavaScriptTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testJavaScriptEnabledIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsJavaScriptTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsJavaScriptTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testJavaScriptEnabledBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsJavaScriptTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsJavaScriptTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testPluginsEnabledNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsPluginsTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsPluginsTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testPluginsEnabledIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsPluginsTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsPluginsTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testPluginsEnabledBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsPluginsTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsPluginsTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testStandardFontFamilyNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsStandardFontFamilyTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsStandardFontFamilyTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testStandardFontFamilyIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsStandardFontFamilyTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsStandardFontFamilyTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testStandardFontFamilyBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsStandardFontFamilyTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsStandardFontFamilyTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDefaultFontSizeNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDefaultFontSizeTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDefaultFontSizeTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDefaultFontSizeIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDefaultFontSizeTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDefaultFontSizeTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDefaultFontSizeBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDefaultFontSizeTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDefaultFontSizeTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDefaultTextEncodingNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDefaultTextEncodingTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDefaultTextEncodingTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDefaultTextEncodingIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDefaultTextEncodingTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDefaultTextEncodingTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDefaultTextEncodingBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDefaultTextEncodingTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDefaultTextEncodingTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUserAgentTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUserAgentTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUserAgentTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUserAgentTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUserAgentTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUserAgentTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDomStorageEnabledNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDomStorageEnabledTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDomStorageEnabledTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDomStorageEnabledIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDomStorageEnabledTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDomStorageEnabledTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testDomStorageEnabledBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsDomStorageEnabledTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsDomStorageEnabledTestHelper(views.getView1(), views.getClient1()));
    }

    class ViewPair {
        private final ContentViewCore view0;
        private final TestAwContentsClient client0;
        private final ContentViewCore view1;
        private final TestAwContentsClient client1;

        ViewPair(ContentViewCore view0, TestAwContentsClient client0,
                 ContentViewCore view1, TestAwContentsClient client1) {
            this.view0 = view0;
            this.client0 = client0;
            this.view1 = view1;
            this.client1 = client1;
        }

        ContentViewCore getView0() {
            return view0;
        }

        TestAwContentsClient getClient0() {
            return client0;
        }

        ContentViewCore getView1() {
            return view1;
        }

        TestAwContentsClient getClient1() {
            return client1;
        }
    }

    /**
     * Runs the tests to check if a setting works properly in the case of
     * multiple WebViews.
     *
     * @param helper0 Test helper for the first ContentView
     * @param helper1 Test helper for the second ContentView
     * @throws Throwable
     */
    private void runPerViewSettingsTest(AwSettingsTestHelper helper0,
                                        AwSettingsTestHelper helper1) throws Throwable {
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper1.setAlteredSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasAlteredValue();

        helper1.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasInitialValue();

        helper1.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasAlteredValue();

        helper0.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasAlteredValue();

        helper1.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();
    }

    private ViewPair createViews(
            boolean firstIsIncognito,
            boolean secondIsIncognito) throws Throwable {
        TestAwContentsClient client0 = new TestAwContentsClient();
        TestAwContentsClient client1 = new TestAwContentsClient();
        return new ViewPair(
            createAwTestContainerViewOnMainSync(
                firstIsIncognito, client0).getContentViewCore(),
            client0,
            createAwTestContainerViewOnMainSync(
                secondIsIncognito, client1).getContentViewCore(),
            client1);
    }
}
