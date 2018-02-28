// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.toolbar.ToolbarModelImpl;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link LocationBarLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarLayoutTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private static final int SEARCH_ICON_RESOURCE = R.drawable.ic_search;

    private static final String SEARCH_TERMS = "machine learning";
    private static final String GOOGLE_SRP_URL = "https://www.google.com/search?q=machine+learning";
    private static final String BING_SRP_URL = "https://www.bing.com/search?q=machine+learning";

    private TestToolbarModel mTestToolbarModel;

    private class TestToolbarModel extends ToolbarModelImpl {
        private String mCurrentUrl;
        private Integer mSecurityLevel;

        public TestToolbarModel() {
            super(null /* bottomSheet */, false /* useModernDesign */);
            initializeWithNative();
        }

        void setCurrentUrl(String url) {
            mCurrentUrl = url;
        }

        void setSecurityLevel(@ConnectionSecurityLevel int securityLevel) {
            mSecurityLevel = securityLevel;
        }

        @Override
        public String getCurrentUrl() {
            if (mCurrentUrl == null) return super.getCurrentUrl();
            return mCurrentUrl;
        }

        @Override
        @ConnectionSecurityLevel
        public int getSecurityLevel() {
            if (mSecurityLevel == null) return super.getSecurityLevel();
            return mSecurityLevel;
        }

        @Override
        public boolean shouldShowGoogleG(String urlBarText) {
            return false;
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestToolbarModel = new TestToolbarModel();
        mTestToolbarModel.setTab(mActivityTestRule.getActivity().getActivityTab(), false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> getLocationBar().setToolbarDataProvider(mTestToolbarModel));
    }

    private void setUrlToPageUrl(LocationBarLayout locationBar) {
        ThreadUtils.runOnUiThreadBlocking(() -> { getLocationBar().updateLoadingState(true); });
    }

    // Partially lifted from TemplateUrlServiceTest.
    private void setSearchEngine(String keyword)
            throws ExecutionException, InterruptedException, TimeoutException {
        CallbackHelper callback = new CallbackHelper();
        Callable<Void> setSearchEngineCallable = new Callable<Void>() {
            @Override
            public Void call() {
                TemplateUrlService.getInstance().runWhenLoaded(() -> {
                    TemplateUrlService.getInstance().setSearchEngine(keyword);
                    callback.notifyCalled();
                });
                return null;
            }
        };
        ThreadUtils.runOnUiThreadBlocking(setSearchEngineCallable);
        callback.waitForCallback("Failed to set search engine", 0);
    }

    private UrlBar getUrlBar() {
        return (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
    }

    private LocationBarLayout getLocationBar() {
        return (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    private TintedImageButton getDeleteButton() {
        return (TintedImageButton) mActivityTestRule.getActivity().findViewById(R.id.delete_button);
    }

    private TintedImageButton getMicButton() {
        return (TintedImageButton) mActivityTestRule.getActivity().findViewById(R.id.mic_button);
    }

    private TintedImageButton getSecurityButton() {
        return (TintedImageButton) mActivityTestRule.getActivity().findViewById(
                R.id.security_button);
    }

    private void setUrlBarTextAndFocus(String text)
            throws ExecutionException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() throws InterruptedException {
                mActivityTestRule.typeInOmnibox(text, false);
                getLocationBar().onUrlFocusChange(true);
                return null;
            }
        });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testNotShowingVoiceSearchButtonIfUrlBarContainsTextAndFlagIsDisabled()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("testing");

        Assert.assertEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertNotEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingVoiceSearchButtonIfUrlBarIsEmptyAndFlagIsDisabled()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("");

        Assert.assertNotEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingVoiceAndDeleteButtonsShowingIfUrlBarContainsText()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("testing");

        Assert.assertEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingOnlyVoiceButtonIfUrlBarIsEmpty()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("");

        Assert.assertNotEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsOnlyShowingSearchTermsIfSrpIsGoogle() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertEquals(SEARCH_TERMS, urlBar.getText().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsOnlyShowingSearchTermsIfSrpIsBing()
            throws ExecutionException, InterruptedException, TimeoutException {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        setSearchEngine("bing.com");
        mTestToolbarModel.setCurrentUrl(BING_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertEquals(SEARCH_TERMS, urlBar.getText().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsNotShowingSearchTermsIfSrpIsBingAndSrpUrlIsGoogle()
            throws ExecutionException, InterruptedException, TimeoutException {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        setSearchEngine("bing.com");
        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertNotEquals(SEARCH_TERMS, urlBar.getText().toString());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testNotShowingSearchIconOnMixedContent() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.NONE);
        setUrlToPageUrl(locationBar);

        TintedImageButton securityButton = getSecurityButton();
        Assert.assertNotEquals(SEARCH_TERMS, urlBar.getText().toString());
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertNotEquals(
                                mTestToolbarModel.getSecurityIconResource(), SEARCH_ICON_RESOURCE));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testIsShowingSearchIconSecureContent() {
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        TintedImageButton securityButton = getSecurityButton();
        Assert.assertEquals(securityButton.getVisibility(), View.VISIBLE);
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(
                                mTestToolbarModel.getSecurityIconResource(), SEARCH_ICON_RESOURCE));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.QUERY_IN_OMNIBOX)
    @Feature({"QueryInOmnibox"})
    public void testNotShowingSearchTermsIfSrpIsGoogleAndFlagIsDisabled() {
        final UrlBar urlBar = getUrlBar();
        final LocationBarLayout locationBar = getLocationBar();

        mTestToolbarModel.setCurrentUrl(GOOGLE_SRP_URL);
        mTestToolbarModel.setSecurityLevel(ConnectionSecurityLevel.SECURE);
        setUrlToPageUrl(locationBar);

        Assert.assertNotEquals(SEARCH_TERMS, urlBar.getText().toString());
    }
}