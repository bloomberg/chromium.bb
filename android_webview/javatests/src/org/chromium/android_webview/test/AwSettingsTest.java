// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.Feature;
import org.chromium.base.test.TestFileUtil;
import org.chromium.base.test.UrlUtils;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.tests.TestContentProvider;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.CallbackHelper;
import org.chromium.content.browser.test.HistoryUtils;

import java.util.concurrent.Callable;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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

        protected void loadUrlSync(String url) throws Throwable {
            AwSettingsTest.this.loadUrlSync(
                mContentViewCore,
                mContentViewClient.getOnPageFinishedHelper(),
                url);
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

    class AwSettingsUserAgentStringTestHelper extends AwSettingsTestHelper<String> {
        private final String mDefaultUa;
        private static final String DEFAULT_UA = "";
        private static final String CUSTOM_UA = "ChromeViewTest";

        AwSettingsUserAgentStringTestHelper(
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
            loadUrlSync(UrlUtils.getTestFileUrl("webview/localStorage.html"));
            assertEquals(
                value == ENABLED ? HAS_LOCAL_STORAGE : NO_LOCAL_STORAGE,
                getTitleOnUiThread());
        }
    }

    class AwSettingsUniversalAccessFromFilesTestHelper extends AwSettingsTestHelper<Boolean> {
        // TODO(mnaganov): Change to "Exception" once
        // https://bugs.webkit.org/show_bug.cgi?id=43504 is fixed.
        private static final String ACCESS_DENIED_TITLE = "undefined";

        AwSettingsUniversalAccessFromFilesTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
            mIframeContainerUrl = UrlUtils.getTestFileUrl("webview/iframe_access.html");
            mIframeUrl = UrlUtils.getTestFileUrl("webview/hello_world.html");
            // The value of the setting depends on the SDK version.
            mContentSettings.setAllowUniversalAccessFromFileURLs(false);
            // If universal access is true, the value of file access doesn't
            // matter. While if universal access is false, having file access
            // enabled will allow file loading.
            mContentSettings.setAllowFileAccessFromFileURLs(false);
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
            return mContentSettings.getAllowUniversalAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowUniversalAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mIframeContainerUrl);
            assertEquals(
                value == ENABLED ? mIframeUrl : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private final String mIframeContainerUrl;
        private final String mIframeUrl;
    }

    class AwSettingsFileAccessFromFilesIframeTestHelper extends AwSettingsTestHelper<Boolean> {
        // TODO(mnaganov): Change to "Exception" once
        // https://bugs.webkit.org/show_bug.cgi?id=43504 is fixed.
        private static final String ACCESS_DENIED_TITLE = "undefined";

        AwSettingsFileAccessFromFilesIframeTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
            mIframeContainerUrl = UrlUtils.getTestFileUrl("webview/iframe_access.html");
            mIframeUrl = UrlUtils.getTestFileUrl("webview/hello_world.html");
            mContentSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mContentSettings.setAllowFileAccessFromFileURLs(false);
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
            return mContentSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowFileAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mIframeContainerUrl);
            assertEquals(
                value == ENABLED ? mIframeUrl : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private final String mIframeContainerUrl;
        private final String mIframeUrl;
    }

    class AwSettingsFileAccessFromFilesXhrTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsFileAccessFromFilesXhrTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient) throws Throwable {
            super(contentViewCore, contentViewClient, true);
            mXhrContainerUrl = UrlUtils.getTestFileUrl("webview/xhr_access.html");
            mContentSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mContentSettings.setAllowFileAccessFromFileURLs(false);
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
            return mContentSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowFileAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mXhrContainerUrl);
            assertEquals(
                value == ENABLED ? ACCESS_GRANTED_TITLE : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private final String mXhrContainerUrl;
    }

    class AwSettingsFileUrlAccessTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";
        private static final String ACCESS_DENIED_TITLE = "about:blank";

        AwSettingsFileUrlAccessTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient,
                int startIndex) throws Throwable {
            super(contentViewCore, contentViewClient, true);
            mIndex = startIndex;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mContentSettings.getAllowFileAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowFileAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // Use query parameters to avoid hitting a cached page.
            String fileUrl = UrlUtils.getTestFileUrl("webview/hello_world.html?id=" + mIndex);
            mIndex += 2;
            loadUrlSync(fileUrl);
            assertEquals(
                value == ENABLED ? ACCESS_GRANTED_TITLE : ACCESS_DENIED_TITLE,
                getTitleOnUiThread());
        }

        private int mIndex;
    }

    class AwSettingsContentUrlAccessTestHelper extends AwSettingsTestHelper<Boolean> {
        private final String mTarget;

        AwSettingsContentUrlAccessTestHelper(
                ContentViewCore contentViewCore,
                TestAwContentsClient contentViewClient,
                int index) throws Throwable {
            super(contentViewCore, contentViewClient, true);
            mTarget = "content_access_" + index;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mContentSettings.getAllowContentAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mContentSettings.setAllowContentAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            AwSettingsTest.this.resetResourceRequestCountInContentProvider(mTarget);
            AwSettingsTest.this.loadUrlSync(
                mContentViewCore,
                mContentViewClient.getOnPageFinishedHelper(),
                AwSettingsTest.this.createContentUrl(mTarget));
            if (value == ENABLED) {
                AwSettingsTest.this.ensureResourceRequestCountInContentProvider(mTarget, 1);
            } else {
                AwSettingsTest.this.ensureResourceRequestCountInContentProvider(mTarget, 0);
            }
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

    // The test verifies that the default user agent string follows the format
    // defined in Android CTS tests:
    //
    // Mozilla/5.0 (Linux;[ U;] Android <version>;[ <language>-<country>;]
    // [<devicemodel>;] Build/<buildID>) AppleWebKit/<major>.<minor> (KHTML, like Gecko)
    // Version/<major>.<minor>[ Mobile] Safari/<major>.<minor>
    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentStringDefault() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        ContentSettings settings = getContentSettingsOnUiThread(contentView);
        final String actualUserAgentString = settings.getUserAgentString();
        final String patternString =
                "Mozilla/5\\.0 \\(Linux;( U;)? Android ([^;]+);( (\\w+)-(\\w+);)?" +
                "\\s?(.*)\\sBuild/(.+)\\) AppleWebKit/(\\d+)\\.(\\d+) \\(KHTML, like Gecko\\) " +
                "Version/\\d+\\.\\d+( Mobile)? Safari/(\\d+)\\.(\\d+)";
        final Pattern userAgentExpr = Pattern.compile(patternString);
        Matcher patternMatcher = userAgentExpr.matcher(actualUserAgentString);
        assertTrue(String.format("User agent string did not match expected pattern. \nExpected " +
                        "pattern:\n%s\nActual:\n%s", patternString, actualUserAgentString),
                        patternMatcher.find());
        // No country-language code token.
        assertEquals(null, patternMatcher.group(3));
        if ("REL".equals(Build.VERSION.CODENAME)) {
            // Model is only added in release builds
            assertEquals(Build.MODEL, patternMatcher.group(6));
            // Release version is valid only in release builds
            assertEquals(Build.VERSION.RELEASE, patternMatcher.group(2));
        }
        assertEquals(Build.ID, patternMatcher.group(7));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentStringOverride() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        ContentSettings settings = getContentSettingsOnUiThread(contentView);
        final String defaultUserAgentString = settings.getUserAgentString();

        // Check that an attempt to reset the default UA string has no effect.
        settings.setUserAgentString(null);
        assertEquals(defaultUserAgentString, settings.getUserAgentString());
        settings.setUserAgentString("");
        assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Check that we can also set the default value.
        settings.setUserAgentString(defaultUserAgentString);
        assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Set a custom UA string, verify that it can be reset back to default.
        final String customUserAgentString = "ContentSettingsTest";
        settings.setUserAgentString(customUserAgentString);
        assertEquals(customUserAgentString, settings.getUserAgentString());
        settings.setUserAgentString(null);
        assertEquals(defaultUserAgentString, settings.getUserAgentString());
    }

    // Verify that the current UA override setting has a priority over UA
    // overrides in navigation history entries.
    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentStringOverrideForHistory() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();
        ContentSettings settings = getContentSettingsOnUiThread(contentView);
        settings.setJavaScriptEnabled(true);
        final String defaultUserAgentString = settings.getUserAgentString();
        final String customUserAgentString = "ContentSettingsTest";
        // We are using different page titles to make sure that we are really
        // going back and forward between them.
        final String pageTemplate =
                "<html><head><title>%s</title></head>" +
                "<body onload='document.title+=navigator.userAgent'></body>" +
                "</html>";
        final String page1Title = "Page1";
        final String page2Title = "Page2";
        final String page1 = String.format(pageTemplate, page1Title);
        final String page2 = String.format(pageTemplate, page2Title);
        settings.setUserAgentString(customUserAgentString);
        loadDataSync(
            contentView, contentClient.getOnPageFinishedHelper(), page1, "text/html", false);
        assertEquals(page1Title + customUserAgentString, getTitleOnUiThread(contentView));
        loadDataSync(
            contentView, contentClient.getOnPageFinishedHelper(), page2, "text/html", false);
        assertEquals(page2Title + customUserAgentString, getTitleOnUiThread(contentView));
        settings.setUserAgentString(null);
        // Must not cause any changes until the next page loading.
        assertEquals(page2Title + customUserAgentString, getTitleOnUiThread(contentView));
        HistoryUtils.goBackSync(getInstrumentation(), contentView, onPageFinishedHelper);
        assertEquals(page1Title + defaultUserAgentString, getTitleOnUiThread(contentView));
        HistoryUtils.goForwardSync(getInstrumentation(), contentView,
                                   onPageFinishedHelper);
        assertEquals(page2Title + defaultUserAgentString, getTitleOnUiThread(contentView));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentStringNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUserAgentStringTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUserAgentStringTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentStringIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUserAgentStringTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUserAgentStringTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUserAgentStringBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUserAgentStringTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUserAgentStringTestHelper(views.getView1(), views.getClient1()));
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

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUniversalAccessFromFilesNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUniversalAccessFromFilesIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testUniversalAccessFromFilesBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsUniversalAccessFromFilesTestHelper(views.getView1(), views.getClient1()));
    }

    // This test verifies that local image resources can be loaded from file:
    // URLs regardless of file access state.
    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesImage() throws Throwable {
        final String imageContainerUrl = UrlUtils.getTestFileUrl("webview/image_access.html");
        final String imageHeight = "16";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        ContentSettings settings = getContentSettingsOnUiThread(contentView);
        settings.setJavaScriptEnabled(true);
        settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setAllowFileAccessFromFileURLs(false);
        loadUrlSync(contentView, contentClient.getOnPageFinishedHelper(), imageContainerUrl);
        assertEquals(imageHeight, getTitleOnUiThread(contentView));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesIframeNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getView0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesIframeIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getView0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesIframeBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getView0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesIframeTestHelper(
                views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesXhrNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesXhrIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileAccessFromFilesXhrBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getView0(), views.getClient0()),
            new AwSettingsFileAccessFromFilesXhrTestHelper(views.getView1(), views.getClient1()));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileUrlAccessNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileUrlAccessTestHelper(views.getView0(), views.getClient0(), 0),
            new AwSettingsFileUrlAccessTestHelper(views.getView1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileUrlAccessIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileUrlAccessTestHelper(views.getView0(), views.getClient0(), 0),
            new AwSettingsFileUrlAccessTestHelper(views.getView1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testFileUrlAccessBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsFileUrlAccessTestHelper(views.getView0(), views.getClient0(), 0),
            new AwSettingsFileUrlAccessTestHelper(views.getView1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testContentUrlAccessNormal() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, NORMAL_VIEW);
        runPerViewSettingsTest(
            new AwSettingsContentUrlAccessTestHelper(views.getView0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessTestHelper(views.getView1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testContentUrlAccessIncognito() throws Throwable {
        ViewPair views = createViews(INCOGNITO_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsContentUrlAccessTestHelper(views.getView0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessTestHelper(views.getView1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences"})
    public void testContentUrlAccessBoth() throws Throwable {
        ViewPair views = createViews(NORMAL_VIEW, INCOGNITO_VIEW);
        runPerViewSettingsTest(
            new AwSettingsContentUrlAccessTestHelper(views.getView0(), views.getClient0(), 0),
            new AwSettingsContentUrlAccessTestHelper(views.getView1(), views.getClient1(), 1));
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences", "Navigation"})
    public void testBlockingContentUrlsFromDataUrls() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        final String target = "content_from_data";
        final String page = "<html><body>" +
                "<img src=\"" +
                createContentUrl(target) + "\">" +
                "</body></html>";
        resetResourceRequestCountInContentProvider(target);
        loadDataSync(
            contentView,
            contentClient.getOnPageFinishedHelper(),
            page,
            "text/html",
            false);
        ensureResourceRequestCountInContentProvider(target, 0);
    }

    private void doTestContentUrlFromFile(boolean addQueryParameters) throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final ContentViewCore contentView =
                createAwTestContainerViewOnMainSync(false, contentClient).getContentViewCore();
        final String target = "content_from_file_" + addQueryParameters;
        Context context = getInstrumentation().getTargetContext();
        final String fileName = context.getCacheDir() + "/" + target + ".html";
        try {
            resetResourceRequestCountInContentProvider(target);
            TestFileUtil.createNewHtmlFile(
                fileName,
                target,
                "<img src=\"" + createContentUrl(target) +
                (addQueryParameters ? "?weather=sunny&life=good" : "") +
                "\">");
            loadUrlSync(
                contentView,
                contentClient.getOnPageFinishedHelper(),
                "file:///" + fileName);
            ensureResourceRequestCountInContentProvider(target, 1);
        } finally {
            TestFileUtil.deleteFile(fileName);
        }
    }

    @SmallTest
    @Feature({"Android-WebView", "Preferences", "Navigation"})
    public void testContentUrlFromFile() throws Throwable {
        doTestContentUrlFromFile(false);
    }

    // Verify that the query parameters are ignored with content URLs.
    @SmallTest
    @Feature({"Android-WebView", "Preferences", "Navigation"})
    public void testContentUrlWithQueryParametersFromFile() throws Throwable {
        doTestContentUrlFromFile(true);
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

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedCount Expected resource requests count
     */
    private void ensureResourceRequestCountInContentProvider(String resource, int expectedCount) {
        Context context = getInstrumentation().getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        assertEquals(expectedCount, actualCount);
    }

    private void resetResourceRequestCountInContentProvider(String resource) {
        Context context = getInstrumentation().getTargetContext();
        TestContentProvider.resetResourceRequestCount(context, resource);
    }

    private String createContentUrl(final String target) {
        return TestContentProvider.createContentUrl(target);
    }
}
