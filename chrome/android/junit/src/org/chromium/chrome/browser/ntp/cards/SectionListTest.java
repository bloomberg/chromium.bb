// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsUnitTestUtils.bindViewHolders;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestions;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.DisableHistogramsRule;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsMetricsReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SectionListTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @CategoryInt
    private static final int CATEGORY1 = 42;
    @CategoryInt
    private static final int CATEGORY2 = CATEGORY1 + 1;

    @Mock
    private SuggestionsUiDelegate mUiDelegate;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;
    @Mock
    private SuggestionsMetricsReporter mMetricsReporter;
    private FakeSuggestionsSource mSuggestionSource;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSuggestionSource = new FakeSuggestionsSource();

        when(mUiDelegate.getSuggestionsSource()).thenReturn(mSuggestionSource);
        when(mUiDelegate.getMetricsReporter()).thenReturn(mMetricsReporter);
    }

    @Test
    @Feature({"Ntp"})
    public void testGetSuggestionRank() {
        // Setup the section list the following way:
        //
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // CATEGORY1  | 0          | 0
        // CATEGORY1  | 1          | 1
        // CATEGORY1  | 2          | 2
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // CATEGORY2  | 0          | 3
        // CATEGORY2  | 1          | 4
        // CATEGORY2  | 2          | 5
        // CATEGORY2  | 3          | 6
        List<SnippetArticle> suggestions1 = registerCategory(mSuggestionSource, CATEGORY1, 3);
        registerCategory(mSuggestionSource, CATEGORY1 + CATEGORY2, 0);
        List<SnippetArticle> suggestions2 = registerCategory(mSuggestionSource, CATEGORY2, 4);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();

        bindViewHolders(sectionList);

        assertThat(suggestions1.get(0).getGlobalRank(), equalTo(0));
        assertThat(suggestions1.get(0).getPerSectionRank(), equalTo(0));
        assertThat(suggestions1.get(2).getGlobalRank(), equalTo(2));
        assertThat(suggestions1.get(2).getPerSectionRank(), equalTo(2));
        assertThat(suggestions2.get(1).getGlobalRank(), equalTo(4));
        assertThat(suggestions2.get(1).getPerSectionRank(), equalTo(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetSuggestionRankWithChanges() {
        // Setup the section list the following way:
        //
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // CATEGORY1  | 0          | 0
        // CATEGORY1  | 1          | 1
        // CATEGORY1  | 2          | 2
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // CATEGORY2  | 0          | 3
        // CATEGORY2  | 1          | 4
        // CATEGORY2  | 2          | 5
        // CATEGORY2  | 3          | 6
        List<SnippetArticle> suggestions1 = registerCategory(mSuggestionSource, CATEGORY1, 3);
        registerCategory(mSuggestionSource, CATEGORY1 + CATEGORY2, 0);
        List<SnippetArticle> suggestions2 = registerCategory(mSuggestionSource, CATEGORY2, 4);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();

        bindViewHolders(sectionList, 0, 5); // Bind until after the third item from |suggestions1|.

        assertThat(suggestions1.get(0).getGlobalRank(), equalTo(0));
        assertThat(suggestions1.get(0).getPerSectionRank(), equalTo(0));
        assertThat(suggestions1.get(2).getGlobalRank(), equalTo(2));
        assertThat(suggestions1.get(2).getPerSectionRank(), equalTo(2));
        assertThat(suggestions2.get(1).getGlobalRank(), equalTo(-1)); // Not bound nor ranked yet.
        assertThat(suggestions2.get(1).getPerSectionRank(), equalTo(-1));

        // Test ranks after changes: remove then add some items.
        @SuppressWarnings("unchecked")
        Callback<String> cb = mock(Callback.class);
        sectionList.dismissItem(2, cb);

        List<SnippetArticle> newSuggestions1 = createDummySuggestions(2, CATEGORY1, "new");
        List<SnippetArticle> newSuggestions2 = createDummySuggestions(2, CATEGORY2, "new");

        sectionList.onMoreSuggestions(CATEGORY1, newSuggestions1.subList(0, 1));
        sectionList.onMoreSuggestions(CATEGORY2, newSuggestions2);

        bindViewHolders(sectionList, 3, sectionList.getItemCount());

        // After the changes we should have:
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // CATEGORY1  | 0          | 0
        // CATEGORY1  | 1          | 1
        //            | -          | -  (deleted)
        // CATEGORY1  | 3          | 3  (new)
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // CATEGORY2  | 0          | 4 (old but not seen until now)
        // CATEGORY2  | 1          | 5 (old but not seen until now)
        // CATEGORY2  | 2          | 6 (old but not seen until now)
        // CATEGORY2  | 3          | 7 (old but not seen until now)
        // CATEGORY2  | 4          | 8 (new)
        // CATEGORY2  | 5          | 9 (new)

        // The new suggestions1 item is seen first and gets the next global rank
        assertThat(newSuggestions1.get(0).getGlobalRank(), equalTo(3));
        assertThat(newSuggestions1.get(0).getPerSectionRank(), equalTo(3));

        // suggestions2 old and new are seen after the new suggestion1 and have higher global ranks
        assertThat(suggestions2.get(1).getGlobalRank(), equalTo(5));
        assertThat(suggestions2.get(1).getPerSectionRank(), equalTo(1));
        assertThat(newSuggestions2.get(1).getGlobalRank(), equalTo(9));
        assertThat(newSuggestions2.get(1).getPerSectionRank(), equalTo(5));

        // Add one more suggestions1
        sectionList.onMoreSuggestions(CATEGORY1, newSuggestions1.subList(1, 2));
        bindViewHolders(sectionList);

        // After the changes we should have:
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // CATEGORY1  | 0          | 0
        // CATEGORY1  | 1          | 1
        //            | -          | -  (deleted)
        // CATEGORY1  | 3          | 3
        // CATEGORY1  | 4          | 10 (new)
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // CATEGORY2  | 0          | 4
        // CATEGORY2  | 1          | 5
        // CATEGORY2  | 2          | 6
        // CATEGORY2  | 3          | 7
        // CATEGORY2  | 4          | 8
        // CATEGORY2  | 5          | 9

        // Old suggestions' ranks should not change.
        assertThat(suggestions1.get(0).getGlobalRank(), equalTo(0));
        assertThat(suggestions1.get(0).getPerSectionRank(), equalTo(0));
        assertThat(suggestions1.get(2).getGlobalRank(), equalTo(2));
        assertThat(suggestions1.get(2).getPerSectionRank(), equalTo(2));
        assertThat(suggestions2.get(1).getGlobalRank(), equalTo(5));
        assertThat(suggestions2.get(1).getPerSectionRank(), equalTo(1));

        // The new suggestions1 should get the last global rank
        assertThat(newSuggestions1.get(1).getGlobalRank(), equalTo(10));
        assertThat(newSuggestions1.get(1).getPerSectionRank(), equalTo(4));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetActionItemRank() {
        registerCategory(mSuggestionSource, CATEGORY1, 0);
        registerCategory(mSuggestionSource,
                new CategoryInfoBuilder(CATEGORY2).withViewAllAction().build(), 3);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();
        bindViewHolders(sectionList);

        assertThat(sectionList.getSectionForTesting(CATEGORY1)
                           .getActionItemForTesting()
                           .getPerSectionRank(),
                equalTo(0));
        assertThat(sectionList.getSectionForTesting(CATEGORY2)
                           .getActionItemForTesting()
                           .getPerSectionRank(),
                equalTo(3));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemovesSectionsWhenUiDelegateDestroyed() {
        registerCategory(mSuggestionSource, CATEGORY1, 1);
        registerCategory(mSuggestionSource,
                new CategoryInfoBuilder(CATEGORY2).withViewAllAction().build(), 3);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();
        bindViewHolders(sectionList);

        ArgumentCaptor<DestructionObserver> argument =
                ArgumentCaptor.forClass(DestructionObserver.class);
        verify(mUiDelegate, atLeastOnce()).addDestructionObserver(argument.capture());

        assertFalse(sectionList.isEmpty());
        SuggestionsSection section = sectionList.getSectionForTesting(CATEGORY1);
        assertNotNull(section);

        // Now destroy the UI and thus notify the SectionList.
        for (DestructionObserver observer : argument.getAllValues()) observer.onDestroy();
        // The section should be removed.
        assertTrue(sectionList.isEmpty());
        // Verify that the section has been detached by notifying its parent about changes. If not
        // detached, it should crash.
        section.notifyItemRangeChanged(0, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testArticlesHeaderHiddenWhenAlone() {
        registerCategory(mSuggestionSource, KnownCategories.ARTICLES, 1);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();
        SuggestionsSection articles = sectionList.getSectionForTesting(KnownCategories.ARTICLES);
        assertFalse(articles.getHeaderItemForTesting().isVisible());
    }

    @Test
    @Feature({"Ntp"})
    public void testRandomSectionHeaderShownWhenAlone() {
        registerCategory(mSuggestionSource, CATEGORY1, 1);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();
        SuggestionsSection section = sectionList.getSectionForTesting(CATEGORY1);
        assertTrue(section.getHeaderItemForTesting().isVisible());
    }

    @Test
    @Feature({"Ntp"})
    public void testArticlesHeaderShownWithOtherSections() {
        registerCategory(mSuggestionSource, KnownCategories.ARTICLES, 1);
        registerCategory(mSuggestionSource, CATEGORY1, 1);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        sectionList.refreshSuggestions();
        SuggestionsSection articles = sectionList.getSectionForTesting(KnownCategories.ARTICLES);
        assertTrue(articles.getHeaderItemForTesting().isVisible());
    }
}
