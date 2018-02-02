// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Unit tests for {@link SuggestionsCarousel}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.CHROME_HOME, ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_CAROUSEL})
public class SuggestionsCarouselTest {
    private static final String URL_STRING = "http://www.test.com";
    private static final String INVALID_URL = "file://URL";

    @Rule
    public TestRule processor = new Features.JUnitProcessor();

    @Mock
    private SuggestionsSource mSuggestionsSource;
    private SuggestionsCarousel mCarousel;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        CommandLine.init(null);

        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getSuggestionsSource()).thenReturn(mSuggestionsSource);

        mCarousel = spy(new SuggestionsCarousel(mock(UiConfig.class), uiDelegate,
                mock(ContextMenuManager.class), mock(OfflinePageBridge.class)));

        mContext = RuntimeEnvironment.application;
    }

    @Test
    public void testInvalidUrl() {
        mCarousel.refresh(mContext, /* newURL = */ null);
        mCarousel.refresh(mContext, /* newURL = */ "");
        mCarousel.refresh(mContext, INVALID_URL);
        verify(mSuggestionsSource, never()).fetchContextualSuggestions(anyString(), any());
    }

    @Test
    public void testValidUrlInitiatesSuggestionsFetch() {
        doReturn(false).when(mCarousel).isContextTheSame(any());

        mCarousel.refresh(mContext, URL_STRING);
        verify(mSuggestionsSource).fetchContextualSuggestions(anyString(), any());
    }
}
