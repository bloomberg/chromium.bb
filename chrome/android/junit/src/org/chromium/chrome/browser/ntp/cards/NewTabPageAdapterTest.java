// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Matchers.greaterThanOrEqualTo;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsUnitTestUtils.bindViewHolders;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsUnitTestUtils.makeUiConfig;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestions;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.explainFailedExpectation;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.viewTypeToString;

import android.content.res.Resources;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowResources;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.DisableHistogramsRule;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.SignInPromo.SigninObserver;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsMetricsReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/**
 * Unit tests for {@link NewTabPageAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageAdapterTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @CategoryInt
    private static final int TEST_CATEGORY = 42;

    private FakeSuggestionsSource mSource;
    private NewTabPageAdapter mAdapter;
    @Mock
    private SigninManager mMockSigninManager;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;
    @Mock
    private SuggestionsUiDelegate mUiDelegate;

    /**
     * Stores information about a section that should be present in the adapter.
     */
    private static class SectionDescriptor {
        public final int mNumSuggestions;
        public final boolean mStatusCard;
        public boolean mActionButton;
        public boolean mProgressItem;
        public SnippetArticle mFirstItem;

        public SectionDescriptor(int numSuggestions) {
            mNumSuggestions = numSuggestions;
            mStatusCard = numSuggestions == 0;
        }

        public SectionDescriptor withActionButton() {
            mActionButton = true;
            return this;
        }

        public SectionDescriptor withProgress() {
            mProgressItem = true;
            return this;
        }

        public SectionDescriptor withFirstItem(SnippetArticle firstItem) {
            mFirstItem = firstItem;
            return this;
        }
    }

    /**
     * Checks the list of items from the adapter against a sequence of expectation, which is
     * expressed as a sequence of calls to the {@link #expect} methods.
     */
    private static class ItemsMatcher { // TODO(pke): Find better name.
        private final TreeNode mTreeNode;
        private int mCurrentIndex;

        public ItemsMatcher(TreeNode root) {
            mTreeNode = root;
        }

        public void expect(@ItemViewType int expectedItemType) {
            if (mCurrentIndex >= mTreeNode.getItemCount()) {
                fail("Expected item of type " + viewTypeToString(expectedItemType)
                        + " but encountered end of list\n"
                        + explainFailedExpectation(mTreeNode, mCurrentIndex, expectedItemType));
            }
            if (mTreeNode.getItemViewType(mCurrentIndex) != expectedItemType) {
                fail("Type mismatch at position " + mCurrentIndex + "\n"
                        + explainFailedExpectation(mTreeNode, mCurrentIndex, expectedItemType));
            }
            mCurrentIndex++;
        }

        public void expect(SectionDescriptor descriptor) {
            expect(ItemViewType.HEADER);

            if (descriptor.mFirstItem != null) {
                if (mTreeNode.getSuggestionAt(mCurrentIndex) != descriptor.mFirstItem) {
                    fail("Wrong item at position " + mCurrentIndex + "\n"
                            + explainFailedExpectation(
                                      mTreeNode, mCurrentIndex, ItemViewType.SNIPPET));
                }
            }

            for (int i = 1; i <= descriptor.mNumSuggestions; i++) {
                expect(ItemViewType.SNIPPET);
            }

            if (descriptor.mStatusCard) {
                expect(ItemViewType.STATUS);
            }

            if (descriptor.mActionButton) {
                // TODO(bauerb): Verify the action.
                expect(ItemViewType.ACTION);
            }

            if (descriptor.mProgressItem) {
                expect(ItemViewType.PROGRESS);
            }
        }

        public void expectEnd() {
            assertEquals(mTreeNode.getItemCount(), mCurrentIndex);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);

        // Set empty variation params for the test.
        CardsVariationParameters.setTestVariationParams(new HashMap<String, String>());

        ChromeFeatureList.setTestEnabledFeatures(Collections.<String>emptySet());

        // Initialise the sign in state. We will be signed in by default in the tests.
        assertFalse(ChromePreferenceManager.getInstance().getNewTabPageSigninPromoDismissed());
        SigninManager.setInstanceForTesting(mMockSigninManager);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(true);
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);

        mSource = new FakeSuggestionsSource();
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        mSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).showIfEmpty().build());

        when(mUiDelegate.getSuggestionsSource()).thenReturn(mSource);
        when(mUiDelegate.getMetricsReporter()).thenReturn(mock(SuggestionsMetricsReporter.class));

        reloadNtp();
    }

    @After
    public void tearDown() {
        CardsVariationParameters.setTestVariationParams(null);
        ChromeFeatureList.setTestEnabledFeatures(null);
        SigninManager.setInstanceForTesting(null);
        ChromePreferenceManager.getInstance().setNewTabPageSigninPromoDismissed(false);
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a suggestions
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoading() {
        assertItemsFor(sectionWithStatusCard().withProgress());

        final int numSuggestions = 3;
        List<SnippetArticle> suggestions = createDummySuggestions(numSuggestions, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        assertItemsFor(section(numSuggestions));
    }

    /**
     * Tests that the adapter keeps listening for suggestion updates.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoadingInitiallyEmpty() {
        // If we don't get anything, we should be in the same situation as the initial one.
        mSource.setSuggestionsForCategory(TEST_CATEGORY, new ArrayList<SnippetArticle>());
        assertItemsFor(sectionWithStatusCard().withProgress());

        // We should load new suggestions when we get notified about them.
        final int numSuggestions = 5;

        List<SnippetArticle> suggestions = createDummySuggestions(numSuggestions, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        assertItemsFor(section(numSuggestions));
    }

    /**
     * Tests that the adapter clears the suggestions when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionClearing() {
        List<SnippetArticle> suggestions = createDummySuggestions(4, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(4));

        // If we get told that the category is enabled, we just leave the current suggestions do not
        // clear them.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        assertItemsFor(section(4));

        // When the category is disabled, the suggestions are cleared and we should go back to
        // the situation with the status card.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.SIGNED_OUT);
        assertItemsFor(sectionWithStatusCard());

        // The adapter should now be waiting for new suggestions.
        suggestions = createDummySuggestions(6, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(6));
    }

    /**
     * Tests that the adapter loads suggestions only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoadingBlock() {
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);

        // By default, status is INITIALIZING, so we can load suggestions.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(3));

        // Add another suggestion.
        suggestions.add(new SnippetArticle(TEST_CATEGORY, "https://site.com/url1", "title1", "pub1",
                "txt1", "https://site.com/url1", 0, 0, 0));

        // When suggestion are disabled, we should not be able to load them.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.SIGNED_OUT);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(sectionWithStatusCard());

        // INITIALIZING lets us load suggestion still.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(sectionWithStatusCard().withProgress());

        // The adapter should now be waiting for new suggestion and the fourth one should appear.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(4));
    }

    /**
     * Tests how the loading indicator reacts to status changes.
     */
    @Test
    @Feature({"Ntp"})
    public void testProgressIndicatorDisplay() {
        SuggestionsSection section =
                mAdapter.getSectionListForTesting().getSectionForTesting(TEST_CATEGORY);
        ProgressItem progress = section.getProgressItemForTesting();

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        assertTrue(progress.isVisible());

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        assertFalse(progress.isVisible());

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE_LOADING);
        assertTrue(progress.isVisible());

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.SIGNED_OUT);
        assertFalse(progress.isVisible());
    }

    /**
     * Tests that the entire section disappears if its status switches to LOADING_ERROR or
     * CATEGORY_EXPLICITLY_DISABLED. Also tests that they are not shown when the NTP reloads.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionClearingWhenUnavailable() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(5));

        // When the category goes away with a hard error, the section is cleared from the UI.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.LOADING_ERROR);
        assertItemsFor();

        // Same when loading a new NTP.
        reloadNtp();
        assertItemsFor();

        // Same for CATEGORY_EXPLICITLY_DISABLED.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        reloadNtp();
        assertItemsFor(section(5));
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertItemsFor();

        reloadNtp();
        assertItemsFor();
    }

    /**
     * Tests that the UI remains untouched if a category switches to NOT_PROVIDED.
     */
    @Test
    @Feature({"Ntp"})
    public void testUIUntouchedWhenNotProvided() {
        List<SnippetArticle> suggestions = createDummySuggestions(4, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(4));

        // When the category switches to NOT_PROVIDED, UI stays the same.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.NOT_PROVIDED);
        mSource.silentlyRemoveCategory(TEST_CATEGORY);
        assertItemsFor(section(4));

        reloadNtp();
        assertItemsFor();
    }

    /**
     * Tests that the UI updates on updated suggestions.
     */
    @Test
    @Feature({"Ntp"})
    public void testUIUpdatesOnNewSuggestionsWhenOtherSectionSeen() {
        List<SnippetArticle> suggestions = createDummySuggestions(4, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        @CategoryInt
        final int otherCategory = TEST_CATEGORY + 1;
        List<SnippetArticle> otherSuggestions = createDummySuggestions(2, otherCategory);
        mSource.setStatusForCategory(otherCategory, CategoryStatus.AVAILABLE);
        mSource.setInfoForCategory(
                otherCategory, new CategoryInfoBuilder(otherCategory).showIfEmpty().build());
        mSource.setSuggestionsForCategory(otherCategory, otherSuggestions);

        reloadNtp();
        assertItemsFor(section(4), section(2));

        // Bind the whole section - indicate that it is being viewed.
        bindViewHolders(mAdapter.getSectionListForTesting().getSectionForTesting(otherCategory));

        List<SnippetArticle> newSuggestions = createDummySuggestions(3, TEST_CATEGORY, "new");
        mSource.setSuggestionsForCategory(TEST_CATEGORY, newSuggestions);
        assertItemsFor(section(3), section(2));

        reloadNtp();
        assertItemsFor(section(3), section(2));
    }

    /** Tests whether a section stays visible if empty, if required. */
    @Test
    @Feature({"Ntp"})
    public void testSectionVisibleIfEmpty() {
        // Part 1: VisibleIfEmpty = true
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).showIfEmpty().build());

        // 1.1 - Initial state
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 1.2 - With suggestions
        List<SnippetArticle> suggestions =
                Collections.unmodifiableList(createDummySuggestions(3, TEST_CATEGORY));
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(3));

        // 1.3 - When all suggestions are dismissed
        SuggestionsSection section =
                mAdapter.getSectionListForTesting().getSectionForTesting(TEST_CATEGORY);
        assertSectionMatches(section(3), section);
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard());

        // Part 2: VisibleIfEmpty = false
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).build());

        // 2.1 - Initial state
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor();

        // 2.2 - With suggestions
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor();

        // 2.3 - When all suggestions are dismissed - N/A, suggestions don't get added.
    }

    /**
     * Tests that the more button is shown for sections that declare it.
     */
    @Test
    @Feature({"Ntp"})
    public void testMoreButton() {
        // Part 1: With "View All" action
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(TEST_CATEGORY,
                new CategoryInfoBuilder(TEST_CATEGORY).withViewAllAction().showIfEmpty().build());

        // 1.1 - Initial state.
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withActionButton().withProgress());

        // 1.2 - With suggestions.
        List<SnippetArticle> suggestions =
                Collections.unmodifiableList(createDummySuggestions(3, TEST_CATEGORY));
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(3).withActionButton());

        // 1.3 - When all suggestions are dismissed.
        SuggestionsSection section =
                mAdapter.getSectionListForTesting().getSectionForTesting(TEST_CATEGORY);
        assertSectionMatches(section(3).withActionButton(), section);
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard().withActionButton());

        // Part 1: Without "View All" action
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).showIfEmpty().build());

        // 2.1 - Initial state.
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 2.2 - With suggestions.
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(3));

        // 2.3 - When all suggestions are dismissed.
        section = mAdapter.getSectionListForTesting().getSectionForTesting(TEST_CATEGORY);
        assertSectionMatches(section(3), section);
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard());
    }

    /**
     * Tests that invalidated suggestions are immediately removed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionInvalidated() {
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(3));
        assertArticlesEqual(suggestions, 2, 5);

        SnippetArticle removed = suggestions.remove(1);
        mSource.fireSuggestionInvalidated(TEST_CATEGORY, removed.mIdWithinCategory);
        assertArticlesEqual(suggestions, 2, 4);
    }

    /**
     * Tests that the UI handles dynamically added (server-side) categories correctly.
     */
    @Test
    @Feature({"Ntp"})
    public void testDynamicCategories() {
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(3));

        int dynamicCategory1 = 1010;
        List<SnippetArticle> dynamics1 = createDummySuggestions(5, dynamicCategory1);
        mSource.setInfoForCategory(dynamicCategory1, new CategoryInfoBuilder(dynamicCategory1)
                                                             .withViewAllAction()
                                                             .build());
        mSource.setStatusForCategory(dynamicCategory1, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(dynamicCategory1, dynamics1);
        reloadNtp();

        assertItemsFor(section(3), section(5).withActionButton());

        int dynamicCategory2 = 1011;
        List<SnippetArticle> dynamics2 = createDummySuggestions(11, dynamicCategory2);
        mSource.setInfoForCategory(dynamicCategory2,
                new CategoryInfoBuilder(dynamicCategory1).build());
        mSource.setStatusForCategory(dynamicCategory2, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(dynamicCategory2, dynamics2);
        reloadNtp();
        assertItemsFor(section(3), section(5).withActionButton(), section(11));
    }

    /**
     * Tests that the order of the categories is kept.
     */
    @Test
    @Feature({"Ntp"})
    public void testCategoryOrder() {
        int[] categories = {TEST_CATEGORY, TEST_CATEGORY + 2, TEST_CATEGORY + 3, TEST_CATEGORY + 4};
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        registerCategory(suggestionsSource, categories[0], 0);
        registerCategory(suggestionsSource, categories[1], 0);
        registerCategory(suggestionsSource, categories[2], 0);
        registerCategory(suggestionsSource, categories[3], 0);
        reloadNtp();

        List<TreeNode> children = mAdapter.getSectionListForTesting().getChildren();
        assertEquals(4, children.size());
        assertEquals(SuggestionsSection.class, children.get(0).getClass());
        assertEquals(categories[0], getCategory(children.get(0)));
        assertEquals(SuggestionsSection.class, children.get(1).getClass());
        assertEquals(categories[1], getCategory(children.get(1)));
        assertEquals(SuggestionsSection.class, children.get(2).getClass());
        assertEquals(categories[2], getCategory(children.get(2)));
        assertEquals(SuggestionsSection.class, children.get(3).getClass());
        assertEquals(categories[3], getCategory(children.get(3)));

        // With a different order.
        suggestionsSource = new FakeSuggestionsSource();
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        registerCategory(suggestionsSource, categories[0], 0);
        registerCategory(suggestionsSource, categories[2], 0);
        registerCategory(suggestionsSource, categories[3], 0);
        registerCategory(suggestionsSource, categories[1], 0);
        reloadNtp();

        children = mAdapter.getSectionListForTesting().getChildren();
        assertEquals(4, children.size());
        assertEquals(SuggestionsSection.class, children.get(0).getClass());
        assertEquals(categories[0], getCategory(children.get(0)));
        assertEquals(SuggestionsSection.class, children.get(1).getClass());
        assertEquals(categories[2], getCategory(children.get(1)));
        assertEquals(SuggestionsSection.class, children.get(2).getClass());
        assertEquals(categories[3], getCategory(children.get(2)));
        assertEquals(SuggestionsSection.class, children.get(3).getClass());
        assertEquals(categories[1], getCategory(children.get(3)));

        // With unknown categories.
        suggestionsSource = new FakeSuggestionsSource();
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        registerCategory(suggestionsSource, categories[0], 0);
        registerCategory(suggestionsSource, categories[2], 0);
        registerCategory(suggestionsSource, categories[3], 0);
        reloadNtp();

        // The adapter is already initialised, it will not accept new categories anymore.
        registerCategory(suggestionsSource, TEST_CATEGORY + 5, 1);
        registerCategory(suggestionsSource, categories[1], 1);

        children = mAdapter.getSectionListForTesting().getChildren();
        assertEquals(3, children.size());
        assertEquals(SuggestionsSection.class, children.get(0).getClass());
        assertEquals(categories[0], getCategory(children.get(0)));
        assertEquals(SuggestionsSection.class, children.get(1).getClass());
        assertEquals(categories[2], getCategory(children.get(1)));
        assertEquals(SuggestionsSection.class, children.get(2).getClass());
        assertEquals(categories[3], getCategory(children.get(2)));
    }

    @Test
    @Feature({"Ntp"})
    public void testChangeNotifications() {
        FakeSuggestionsSource suggestionsSource = spy(new FakeSuggestionsSource());
        registerCategory(suggestionsSource, TEST_CATEGORY, 3);
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);

        @SuppressWarnings("unchecked")
        Callback<String> itemDismissedCallback = mock(Callback.class);

        reloadNtp();
        AdapterDataObserver dataObserver = mock(AdapterDataObserver.class);
        mAdapter.registerAdapterDataObserver(dataObserver);

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2-4 | Sugg*3
        // 5   | Action
        // 6   | Footer
        // 7   | Spacer

        // Dismiss the second suggestion of the second section.
        mAdapter.dismissItem(3, itemDismissedCallback);
        verify(itemDismissedCallback).onResult(anyString());
        verify(dataObserver).onItemRangeRemoved(3, 1);
        verify(dataObserver).onItemRangeChanged(6, 1, null);

        // Make sure the call with the updated position works properly.
        mAdapter.dismissItem(3, itemDismissedCallback);
        verify(itemDismissedCallback, times(2)).onResult(anyString());
        verify(dataObserver, times(2)).onItemRangeRemoved(3, 1);
        verify(dataObserver).onItemRangeChanged(5, 1, null);

        // Dismiss the last suggestion in the section. We should now show the status card.
        reset(dataObserver);
        mAdapter.dismissItem(2, itemDismissedCallback);
        verify(itemDismissedCallback, times(3)).onResult(anyString());
        verify(dataObserver).onItemRangeRemoved(2, 1); // Suggestion removed
        verify(dataObserver).onItemRangeChanged(4, 1, null); // Spacer refresh
        verify(dataObserver).onItemRangeInserted(2, 1); // Status card added
        verify(dataObserver).onItemRangeChanged(5, 1, null); // Spacer refresh

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Status
        // 3   | Action
        // 4   | Progress Indicator
        // 5   | Footer
        // 6   | Spacer

        final int newSuggestionCount = 7;
        reset(dataObserver);
        suggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY, createDummySuggestions(newSuggestionCount, TEST_CATEGORY));
        verify(dataObserver).onItemRangeInserted(2, newSuggestionCount);
        verify(dataObserver).onItemRangeChanged(5 + newSuggestionCount, 1, null); // Spacer refresh
        verify(dataObserver).onItemRangeRemoved(2 + newSuggestionCount, 1);
        verify(dataObserver).onItemRangeChanged(4 + newSuggestionCount, 1, null); // Spacer refresh

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2-8 | Sugg*7
        // 9   | Action
        // 10  | Footer
        // 11  | Spacer

        reset(dataObserver);
        suggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY, createDummySuggestions(0, TEST_CATEGORY));
        mAdapter.getSectionListForTesting().onCategoryStatusChanged(
                TEST_CATEGORY, CategoryStatus.SIGNED_OUT);
        verify(dataObserver).onItemRangeRemoved(2, newSuggestionCount);
        verify(dataObserver).onItemRangeChanged(4, 1, null); // Spacer refresh
        verify(dataObserver).onItemRangeInserted(2, 1); // Status card added
        verify(dataObserver).onItemRangeChanged(5, 1, null); // Spacer refresh
    }

    @Test
    @Feature({"Ntp"})
    public void testSigninPromo() {
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        ArgumentCaptor<DestructionObserver> observers =
                ArgumentCaptor.forClass(DestructionObserver.class);

        doNothing().when(mUiDelegate).addDestructionObserver(observers.capture());

        reloadNtp();
        assertTrue(isSignInPromoVisible());

        // Note: As currently implemented, these two variables should point to the same object, a
        // SignInPromo.SigninObserver
        SignInStateObserver signInStateObserver = null;
        SignInAllowedObserver signInAllowedObserver = null;
        for (DestructionObserver observer : observers.getAllValues()) {
            if (observer instanceof SignInStateObserver) {
                signInStateObserver = (SignInStateObserver) observer;
            }
            if (observer instanceof SignInAllowedObserver) {
                signInAllowedObserver = (SignInAllowedObserver) observer;
            }
        }

        signInStateObserver.onSignedIn();
        assertFalse(isSignInPromoVisible());

        signInStateObserver.onSignedOut();
        assertTrue(isSignInPromoVisible());

        when(mMockSigninManager.isSignInAllowed()).thenReturn(false);
        signInAllowedObserver.onSignInAllowedChanged();
        assertFalse(isSignInPromoVisible());

        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        signInAllowedObserver.onSignInAllowedChanged();
        assertTrue(isSignInPromoVisible());
    }

    @Test
    @Feature({"Ntp"})
    @Config(shadows = MyShadowResources.class)
    public void testSigninPromoDismissal() {
        final String signInPromoText = "sign in";
        when(MyShadowResources.sResources.getText(R.string.snippets_disabled_generic_prompt))
                .thenReturn(signInPromoText);

        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        ChromePreferenceManager.getInstance().setNewTabPageSigninPromoDismissed(false);
        reloadNtp();

        final int signInPromoPosition = mAdapter.getFirstPositionForType(ItemViewType.PROMO);
        assertNotEquals(RecyclerView.NO_POSITION, signInPromoPosition);
        @SuppressWarnings("unchecked")
        Callback<String> itemDismissedCallback = mock(Callback.class);
        mAdapter.dismissItem(signInPromoPosition, itemDismissedCallback);

        verify(itemDismissedCallback).onResult(anyString());
        assertFalse(isSignInPromoVisible());
        assertTrue(ChromePreferenceManager.getInstance().getNewTabPageSigninPromoDismissed());

        reloadNtp();
        assertFalse(isSignInPromoVisible());
    }

    @Test
    @Feature({"Ntp"})
    public void testAllDismissedVisibility() {
        ArgumentCaptor<DestructionObserver> observers =
                ArgumentCaptor.forClass(DestructionObserver.class);

        verify(mUiDelegate, atLeastOnce()).addDestructionObserver(observers.capture());

        SigninObserver signinObserver = null;
        for (DestructionObserver observer : observers.getAllValues()) {
            if (observer instanceof SigninObserver) {
                signinObserver = (SigninObserver) observer;
            }
        }

        @SuppressWarnings("unchecked")
        Callback<String> itemDismissedCallback = mock(Callback.class);

        // By default, there is no All Dismissed item.
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Status
        // 3   | Progress Indicator
        // 4   | Footer
        // 5   | Spacer
        assertEquals(4, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // When we remove the section, the All Dismissed item should be there.
        mAdapter.dismissItem(2, itemDismissedCallback);

        verify(itemDismissedCallback).onResult(anyString());

        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | All Dismissed
        // 2   | Spacer
        assertEquals(
                RecyclerView.NO_POSITION, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(1, mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // On Sign out, the sign in promo should come and the All Dismissed item be removed.
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        signinObserver.onSignedOut();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Sign In Promo
        // 2   | Footer
        // 3   | Spacer
        assertEquals(2, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // When sign in is disabled, the promo is removed and the All Dismissed item can come back.
        when(mMockSigninManager.isSignInAllowed()).thenReturn(false);
        signinObserver.onSignInAllowedChanged();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | All Dismissed
        // 2   | Spacer
        assertEquals(
                RecyclerView.NO_POSITION, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(1, mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // Re-enabling sign in should only bring the promo back, thus removing the AllDismissed item
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        signinObserver.onSignInAllowedChanged();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Sign In Promo
        // 2   | Footer
        // 3   | Spacer
        assertEquals(ItemViewType.FOOTER, mAdapter.getItemViewType(2));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // Prepare some suggestions. They should not load because the category is dismissed on
        // the current NTP.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, createDummySuggestions(1, TEST_CATEGORY));
        mSource.setInfoForCategory(TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).build());
        assertEquals(4, mAdapter.getItemCount()); // TODO(dgn): rewrite with section descriptors.

        // On Sign in, we should reset the sections, bring back suggestions instead of the All
        // Dismissed item.
        mAdapter.getSectionListForTesting().refreshSuggestions();
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        signinObserver.onSignedIn();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Suggestion
        // 4   | Footer
        // 5   | Spacer
        assertEquals(3, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));
    }

    /**
     * Robolectric shadow to mock out calls to {@link Resources#getString}.
     */
    @Implements(Resources.class)
    public static class MyShadowResources extends ShadowResources {
        public static final Resources sResources = mock(Resources.class);

        @Override
        @Implementation
        public CharSequence getText(int id) {
            return sResources.getText(id);
        }
    }

    /**
     * Asserts that the given {@link TreeNode} is a {@link SuggestionsSection} that matches the
     * given {@link SectionDescriptor}.
     * @param descriptor The section descriptor to match against.
     * @param section The section from the adapter.
     */
    private void assertSectionMatches(SectionDescriptor descriptor, SuggestionsSection section) {
        ItemsMatcher matcher = new ItemsMatcher(section);
        matcher.expect(descriptor);
        matcher.expectEnd();
    }

    /**
     * Asserts that {@link #mAdapter}.{@link NewTabPageAdapter#getItemCount()} corresponds to an NTP
     * with the given sections in it.
     *
     * @param descriptors A list of descriptors, each describing a section that should be present on
     *                    the UI.
     */
    private void assertItemsFor(SectionDescriptor... descriptors) {
        ItemsMatcher matcher = new ItemsMatcher(mAdapter.getRootForTesting());
        matcher.expect(ItemViewType.ABOVE_THE_FOLD);
        for (SectionDescriptor descriptor : descriptors) matcher.expect(descriptor);
        if (descriptors.length == 0) {
            matcher.expect(ItemViewType.ALL_DISMISSED);
        } else {
            matcher.expect(ItemViewType.FOOTER);
        }
        matcher.expect(ItemViewType.SPACING);
        matcher.expectEnd();
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section with
     * {@code numSuggestions} cards in it.
     * @param numSuggestions The number of suggestions in the section. If there are zero, use either
     *                       no section at all (if it is not displayed) or
     *                       {@link #sectionWithStatusCard()}.
     * @return A descriptor for the section.
     */
    private SectionDescriptor section(int numSuggestions) {
        assert numSuggestions > 0;
        return new SectionDescriptor(numSuggestions);
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section that has no
     * suggestions, but a status card to be displayed.
     * @return A descriptor for the section.
     */
    private SectionDescriptor sectionWithStatusCard() {
        return new SectionDescriptor(0);
    }

    private void reloadNtp() {
        mAdapter = new NewTabPageAdapter(mUiDelegate, mock(View.class), makeUiConfig(),
                mOfflinePageBridge, mock(ContextMenuManager.class), /* tileGroupDelegate =
                */ null);
        mAdapter.refreshSuggestions();
    }

    private void assertArticlesEqual(List<SnippetArticle> articles, int start, int end) {
        assertThat(mAdapter.getItemCount(), greaterThanOrEqualTo(end));
        for (int i = start; i < end; i++) {
            assertEquals(articles.get(i - start), mAdapter.getSuggestionAt(i));
        }
    }

    private boolean isSignInPromoVisible() {
        return mAdapter.getFirstPositionForType(ItemViewType.PROMO) != RecyclerView.NO_POSITION;
    }

    private int getCategory(TreeNode item) {
        return ((SuggestionsSection) item).getCategory();
    }
}
