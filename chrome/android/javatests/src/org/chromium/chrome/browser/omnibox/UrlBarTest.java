// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Editable;
import android.text.TextUtils;
import android.view.inputmethod.BaseInputConnection;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.StubAutocompleteController;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the URL bar UI component.
 */
public class UrlBarTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    // 9000+ chars of goodness
    private static final String HUGE_URL =
            "data:text/plain,H"
            + new String(new char[9000]).replace('\0', 'u')
            + "ge!";

    public UrlBarTest() {
        super(ChromeActivity.class);
    }

    private UrlBar getUrlBar() {
        return (UrlBar) getActivity().findViewById(R.id.url_bar);
    }

    private void stubLocationBarAutocomplete() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                LocationBarLayout locationBar =
                        (LocationBarLayout) getActivity().findViewById(R.id.location_bar);
                locationBar.setAutocompleteController(new StubAutocompleteController());
            }
        });
    }

    private static class AutocompleteState {
        public final boolean hasAutocomplete;
        public final String textWithoutAutocomplete;
        public final String textWithAutocomplete;

        public AutocompleteState(
                boolean hasAutocomplete, String textWithoutAutocomplete,
                String textWithAutocomplete) {
            this.hasAutocomplete = hasAutocomplete;
            this.textWithoutAutocomplete = textWithoutAutocomplete;
            this.textWithAutocomplete = textWithAutocomplete;
        }
    }

    private Editable getUrlBarText(final UrlBar urlBar) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Editable>() {
            @Override
            public Editable call() throws Exception {
                return urlBar.getText();
            }
        });
    }

    private AutocompleteState getAutocompleteState(
            final UrlBar urlBar, final Runnable action) {
        final AtomicBoolean hasAutocomplete = new AtomicBoolean();
        final AtomicReference<String> textWithoutAutocomplete = new AtomicReference<String>();
        final AtomicReference<String> textWithAutocomplete = new AtomicReference<String>();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (action != null) action.run();
                textWithoutAutocomplete.set(urlBar.getTextWithoutAutocomplete());
                textWithAutocomplete.set(urlBar.getQueryText());
                hasAutocomplete.set(urlBar.hasAutocomplete());
            }
        });

        return new AutocompleteState(
                hasAutocomplete.get(), textWithoutAutocomplete.get(), textWithAutocomplete.get());
    }

    private void setTextAndVerifyNoAutocomplete(final UrlBar urlBar, final String text) {
        AutocompleteState state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.setText(text);
            }
        });

        assertEquals(text, state.textWithoutAutocomplete);
        assertEquals(text, state.textWithAutocomplete);
        assertFalse(state.hasAutocomplete);
    }

    private void setAutocomplete(final UrlBar urlBar,
            final String userText, final String autocompleteText) {
        AutocompleteState state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.setAutocompleteText(userText, autocompleteText);
            }
        });

        assertEquals(userText, state.textWithoutAutocomplete);
        assertEquals(userText + autocompleteText, state.textWithAutocomplete);
        assertTrue(state.hasAutocomplete);
    }

    private AutocompleteState setSelection(
            final UrlBar urlBar, final int selectionStart, final int selectionEnd) {
        return getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.setSelection(selectionStart, selectionEnd);
            }
        });
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testRefocusing() throws InterruptedException {
        startMainActivityOnBlankPage();
        UrlBar urlBar = getUrlBar();
        assertFalse(OmniboxTestUtils.doesUrlBarHaveFocus(urlBar));
        OmniboxTestUtils.checkUrlBarRefocus(urlBar, 5);
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testAutocompleteUpdatedOnSetText() throws InterruptedException {
        startMainActivityOnBlankPage();
        stubLocationBarAutocomplete();
        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        // Verify that setting a new string will clear the autocomplete.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        setTextAndVerifyNoAutocomplete(urlBar, "new string");

        // Replace part of the non-autocomplete text and see that the autocomplete is cleared.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        AutocompleteState state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.setText(urlBar.getText().replace(1, 2, "a"));
            }
        });
        assertFalse(state.hasAutocomplete);
        assertEquals("tasting is fun", state.textWithoutAutocomplete);
        assertEquals("tasting is fun", state.textWithAutocomplete);

        // Replace part of the autocomplete text and see that the autocomplete is cleared.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.setText(urlBar.getText().replace(8, 10, "no"));
            }
        });
        assertFalse(state.hasAutocomplete);
        assertEquals("testing no fun", state.textWithoutAutocomplete);
        assertEquals("testing no fun", state.textWithAutocomplete);
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testAutocompleteUpdatedOnSelection() throws InterruptedException {
        startMainActivityOnBlankPage();
        stubLocationBarAutocomplete();
        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        // Verify that setting a selection before the autocomplete clears it.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        AutocompleteState state = setSelection(urlBar, 1, 1);
        assertFalse(state.hasAutocomplete);
        assertEquals("test", state.textWithoutAutocomplete);
        assertEquals("test", state.textWithAutocomplete);

        // Verify that setting a selection range before the autocomplete clears it.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 0, 4);
        assertFalse(state.hasAutocomplete);
        assertEquals("test", state.textWithoutAutocomplete);
        assertEquals("test", state.textWithAutocomplete);

        // Verify that setting a selection at the start of the autocomplete clears it.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 4, 4);
        assertFalse(state.hasAutocomplete);
        assertEquals("test", state.textWithoutAutocomplete);
        assertEquals("test", state.textWithAutocomplete);

        // Verify that setting a selection range that covers a portion of the non-autocomplete
        // and autocomplete text does not delete the autocomplete text.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 2, 5);
        assertFalse(state.hasAutocomplete);
        assertEquals("testing is fun", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);

        // Verify that setting a selection range that over the entire string does not delete
        // the autocomplete text.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 0, 14);
        assertFalse(state.hasAutocomplete);
        assertEquals("testing is fun", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);

        // Verify that setting a selection at the end of the text does not delete the autocomplete
        // text.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 14, 14);
        assertFalse(state.hasAutocomplete);
        assertEquals("testing is fun", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);

        // Verify that setting a selection in the middle of the autocomplete text does not delete
        // the autocomplete text.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 9, 9);
        assertFalse(state.hasAutocomplete);
        assertEquals("testing is fun", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);

        // Verify that setting a selection range in the middle of the autocomplete text does not
        // delete the autocomplete text.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 8, 11);
        assertFalse(state.hasAutocomplete);
        assertEquals("testing is fun", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);

        // Verify that setting the same selection does not clear the autocomplete text.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        state = setSelection(urlBar, 4, 14);
        assertTrue(state.hasAutocomplete);
        assertEquals("test", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testSelectionChangesIgnoredInBatchMode() throws InterruptedException {
        startMainActivityOnBlankPage();
        stubLocationBarAutocomplete();
        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.beginBatchEdit();
            }
        });
        // Ensure the autocomplete is not modified if in batch mode.
        AutocompleteState state = setSelection(urlBar, 1, 1);
        assertTrue(state.hasAutocomplete);
        assertEquals("test", state.textWithoutAutocomplete);
        assertEquals("testing is fun", state.textWithAutocomplete);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.endBatchEdit();
            }
        });

        // Ensure that after batch mode has ended that the autocomplete is cleared due to the
        // invalid selection range.
        state = getAutocompleteState(urlBar, null);
        assertFalse(state.hasAutocomplete);
        assertEquals("test", state.textWithoutAutocomplete);
        assertEquals("test", state.textWithAutocomplete);
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testBatchModeChangesTriggerCorrectSuggestions() throws InterruptedException {
        startMainActivityOnBlankPage();

        final AtomicReference<String> requestedAutocompleteText = new AtomicReference<String>();
        final StubAutocompleteController controller = new StubAutocompleteController() {
            @Override
            public void start(Profile profile, String url, String text,
                    boolean preventInlineAutocomplete) {
                requestedAutocompleteText.set(text);
            }

            @Override
            public void start(Profile profile, String url, String text, int cursorPosition,
                    boolean preventInlineAutocomplete) {
                requestedAutocompleteText.set(text);
            }
        };

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                LocationBarLayout locationBar =
                        (LocationBarLayout) getActivity().findViewById(R.id.location_bar);
                locationBar.setAutocompleteController(controller);
            }
        });

        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.beginBatchEdit();
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.commitText("y", 1);
            }
        });
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.endBatchEdit();
            }
        });

        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("testy", requestedAutocompleteText.get());
            }
        });
        assertEquals("Autocomplete sent incorrectly.", "testy", requestedAutocompleteText.get());
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testAutocompleteUpdatedOnDefocus() throws InterruptedException {
        startMainActivityOnBlankPage();
        stubLocationBarAutocomplete();
        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);

        // Verify that defocusing the UrlBar clears the autocomplete.
        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, false);
        AutocompleteState state = getAutocompleteState(urlBar, null);
        assertFalse(state.hasAutocomplete);
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testAutocompleteClearedOnComposition()
            throws InterruptedException, ExecutionException {
        startMainActivityOnBlankPage();
        stubLocationBarAutocomplete();
        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        setTextAndVerifyNoAutocomplete(urlBar, "test");
        setAutocomplete(urlBar, "test", "ing is fun");

        assertNotNull(urlBar.mInputConnection);
        AutocompleteState state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.setComposingText("ing compose", 4);
            }
        });
        assertFalse(state.hasAutocomplete);

        Editable urlText = getUrlBarText(urlBar);
        assertEquals("testing compose", urlText.toString());
        // TODO(tedchoc): Investigate why this fails on x86.
        //assertEquals(4, BaseInputConnection.getComposingSpanStart(urlText));
        //assertEquals(15, BaseInputConnection.getComposingSpanEnd(urlText));
    }

    @SmallTest
    @Feature("Omnibox")
    public void testDelayedCompositionCorrectedWithAutocomplete()
            throws InterruptedException, ExecutionException {
        startMainActivityOnBlankPage();
        stubLocationBarAutocomplete();

        final UrlBar urlBar = getUrlBar();
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        assertNotNull(urlBar.mInputConnection);

        // Test with a single autocomplete

        setTextAndVerifyNoAutocomplete(urlBar, "chrome://f");
        setAutocomplete(urlBar, "chrome://f", "lags");

        AutocompleteState state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.setComposingRegion(13, 14);
                urlBar.mInputConnection.setComposingText("f", 1);
            }
        });
        assertFalse(state.hasAutocomplete);

        Editable urlText = getUrlBarText(urlBar);
        assertEquals("chrome://f", urlText.toString());
        assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 9);
        assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 10);

        // Test with > 1 characters in composition.

        setTextAndVerifyNoAutocomplete(urlBar, "chrome://fl");
        setAutocomplete(urlBar, "chrome://fl", "ags");

        state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.setComposingRegion(12, 14);
                urlBar.mInputConnection.setComposingText("fl", 1);
            }
        });
        assertFalse(state.hasAutocomplete);

        urlText = getUrlBarText(urlBar);
        assertEquals("chrome://fl", urlText.toString());
        assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 9);
        assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 11);

        // Test with non-matching composition.  Should just append to the URL text.

        setTextAndVerifyNoAutocomplete(urlBar, "chrome://f");
        setAutocomplete(urlBar, "chrome://f", "lags");

        state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.setComposingRegion(13, 14);
                urlBar.mInputConnection.setComposingText("g", 1);
            }
        });
        assertFalse(state.hasAutocomplete);

        urlText = getUrlBarText(urlBar);
        assertEquals("chrome://fg", urlText.toString());
        assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 10);
        assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 11);

        // Test with composition text that matches the entire text w/o autocomplete.

        setTextAndVerifyNoAutocomplete(urlBar, "chrome://f");
        setAutocomplete(urlBar, "chrome://f", "lags");

        state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.setComposingRegion(13, 14);
                urlBar.mInputConnection.setComposingText("chrome://f", 1);
            }
        });
        assertFalse(state.hasAutocomplete);

        urlText = getUrlBarText(urlBar);
        assertEquals("chrome://f", urlText.toString());
        assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 0);
        assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 10);

        // Test with composition text longer than the URL text.  Shouldn't crash and should
        // just append text.

        setTextAndVerifyNoAutocomplete(urlBar, "chrome://f");
        setAutocomplete(urlBar, "chrome://f", "lags");

        state = getAutocompleteState(urlBar, new Runnable() {
            @Override
            public void run() {
                urlBar.mInputConnection.setComposingRegion(13, 14);
                urlBar.mInputConnection.setComposingText("blahblahblah", 1);
            }
        });
        assertFalse(state.hasAutocomplete);

        urlText = getUrlBarText(urlBar);
        assertEquals("chrome://fblahblahblah", urlText.toString());
        assertEquals(BaseInputConnection.getComposingSpanStart(urlText), 10);
        assertEquals(BaseInputConnection.getComposingSpanEnd(urlText), 22);
    }

    /**
     * Test to verify the omnibox can take focus during startup before native libraries have
     * loaded.
     */
    @SmallTest
    @Feature({"Omnibox"})
    public void testFocusingOnStartup() throws InterruptedException {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        prepareUrlIntent(intent, "about:blank");
        startActivityCompletely(intent);

        UrlBar urlBar = getUrlBar();
        assertNotNull(urlBar);
        OmniboxTestUtils.toggleUrlBarFocus(urlBar, true);
        assertTrue(OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true));
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testCopyHuge() throws InterruptedException {
        startMainActivityWithURL(HUGE_URL);
        OmniboxTestUtils.toggleUrlBarFocus(getUrlBar(), true);
        assertEquals(HUGE_URL, copyUrlToClipboard(android.R.id.copy));
    }

    @SmallTest
    @Feature({"Omnibox"})
    public void testCutHuge() throws InterruptedException {
        startMainActivityWithURL(HUGE_URL);
        OmniboxTestUtils.toggleUrlBarFocus(getUrlBar(), true);
        assertEquals(HUGE_URL, copyUrlToClipboard(android.R.id.cut));
    }

    /**
     * Clears the clipboard, executes specified action on the omnibox and
     * returns clipboard's content. Action can be either android.R.id.copy
     * or android.R.id.cut.
     */
    private String copyUrlToClipboard(final int action) {
        ClipboardManager clipboardManager = (ClipboardManager)
                getActivity().getSystemService(Context.CLIPBOARD_SERVICE);

        clipboardManager.setPrimaryClip(ClipData.newPlainText(null, ""));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(getUrlBar().onTextContextMenuItem(action));
            }
        });

        ClipData clip = clipboardManager.getPrimaryClip();
        CharSequence text = (clip != null && clip.getItemCount() != 0)
                ? clip.getItemAt(0).getText() : null;
        return text != null ? text.toString() : null;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Each test will start the activity.
    }
}
