// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a simple framework for a test of an Application. See
 * {@link android.test.ApplicationTestCase ApplicationTestCase} for more
 * information on how to write and extend Application tests.
 */

package org.chromium.chrome.browser.widget.findinpage;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.Spannable;
import android.text.style.StyleSpan;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/**
 * Find in page tests.
 */
public class FindTest extends ChromeTabbedActivityTestBase {
    private static final String FILEPATH = "/chrome/test/data/android/find/test.html";

    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Returns the FindResults text.
     */
    private String waitForFindResults(String expectedResult) {
        final TextView findResults = (TextView) getActivity().findViewById(R.id.find_status);
        assertNotNull(expectedResult);
        assertNotNull(findResults);
        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedResult, new Callable<CharSequence>() {
                        @Override
                        public CharSequence call() {
                            return findResults.getText();
                        }
                    }));
        return findResults.getText().toString();
    }

    /**
     * Find in page by invoking the 'find in page' menu item.
     *
     * @throws InterruptedException
     */
    private void findInPageFromMenu() throws InterruptedException {
        MenuUtils.invokeCustomMenuActionSync(getInstrumentation(),
                getActivity(), R.id.find_in_page_id);

        waitForFindInPageVisibility(true);
    }

    private void waitForFindInPageVisibility(final boolean visible) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                FindToolbar findToolbar = (FindToolbar) getActivity().findViewById(
                        R.id.find_toolbar);

                boolean isVisible = findToolbar != null && findToolbar.isShown();
                return (visible == isVisible) && !findToolbar.isAnimating();
            }
        });
    }

    private String findStringInPage(final String query, String expectedResult)
            throws InterruptedException {
        findInPageFromMenu();
        // FindToolbar should automatically get focus.
        final TextView findQueryText = getFindQueryText();
        assertTrue("FindToolbar should have focus", findQueryText.hasFocus());

        // We have to send each key 1-by-1 to trigger the right listeners in the toolbar.
        KeyCharacterMap keyCharacterMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        final KeyEvent[] events = keyCharacterMap.getEvents(query.toCharArray());
        assertNotNull(events);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < events.length; i++) {
                    if (!findQueryText.dispatchKeyEventPreIme(events[i])) {
                        findQueryText.dispatchKeyEvent(events[i]);
                    }
                }
            }
        });
        return waitForFindResults(expectedResult);
    }

    private void loadTestAndVerifyFindInPage(String query, String expectedResult)
            throws InterruptedException {
        loadUrl(mTestServer.getURL(FILEPATH));
        String findResults = findStringInPage(query, expectedResult);
        assertTrue("Expected: " + expectedResult + " Got: " + findResults + " for: "
                + mTestServer.getURL(FILEPATH),
                findResults.contains(expectedResult));
    }

    private FindToolbar getFindToolbar() {
        final FindToolbar findToolbar = (FindToolbar) getActivity().findViewById(R.id.find_toolbar);
        assertNotNull("FindToolbar not found", findToolbar);
        return findToolbar;
    }

    private EditText getFindQueryText() {
        final EditText findQueryText = (EditText) getActivity().findViewById(R.id.find_query);
        assertNotNull("FindQueryText not found", findQueryText);
        return findQueryText;
    }

    /**
     * Verify Find In Page is not case sensitive.
     */
    @MediumTest
    @Feature({"FindInPage", "Main"})
    @RetryOnFailure
    public void testFind() throws InterruptedException {
        loadTestAndVerifyFindInPage("pitts", "1/7");
    }

    /**
     * Verify Find In Page with just one result.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFind101() throws InterruptedException {
        loadTestAndVerifyFindInPage("it", "1/101");
    }

    /**
     * Verify Find In Page with a multi-line string.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindMultiLine() throws InterruptedException {
        String multiLineSearchTerm = "This is the text of this document.\n"
                + " I am going to write the word \'Pitts\' 7 times. (That was one.)";
        loadTestAndVerifyFindInPage(multiLineSearchTerm, "1/1");
    }

    /**
     * Test for Find In Page with a multi-line string. Search string has an extra character
     * added to the end so it should not be found.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindMultiLineFalse() throws InterruptedException {
        String multiLineSearchTerm = "aThis is the text of this document.\n"
                + " I am going to write the word \'Pitts\' 7 times. (That was one.)";
        loadTestAndVerifyFindInPage(multiLineSearchTerm, "0/0");
    }

    /**
     * Verify Find In Page Next button.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindNext() throws InterruptedException {
        String query = "pitts";
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        singleClickView(getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        for (int i = 2; i <= 7; i++) {
            singleClickView(getActivity().findViewById(R.id.find_next_button));
        }
        waitForFindResults("1/7");
    }

    /**
     * Verify Find In Page Next/Previous button.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindNextPrevious() throws InterruptedException {
        String query = "pitts";
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        singleClickView(getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        singleClickView(getActivity().findViewById(R.id.find_prev_button));
        waitForFindResults("1/7");
    }

    /**
     * Verify that Find in page toolbar is dismissed on entering fullscreen.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFullscreen() throws InterruptedException {
        loadTestAndVerifyFindInPage("pitts", "1/7");

        Tab tab = getActivity().getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, true, getActivity());
        waitForFindInPageVisibility(false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(tab, false, getActivity());
        waitForFindInPageVisibility(false);
    }

    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testResultsBarInitiallyVisible() throws InterruptedException {
        loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();
        final FindToolbar findToolbar = getFindToolbar();
        final View resultBar = findToolbar.getFindResultBar();
        assertNotNull(resultBar);
        assertEquals(View.VISIBLE, resultBar.getVisibility());
    }

    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testResultsBarVisibleAfterTypingText() throws InterruptedException {
        loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();
        final FindToolbar findToolbar = getFindToolbar();
        final View resultBar = findToolbar.getFindResultBar();
        assertNotNull(resultBar);
        final TextView findQueryText = getFindQueryText();

        KeyUtils.singleKeyEventView(getInstrumentation(), findQueryText, KeyEvent.KEYCODE_T);
        assertEquals(View.VISIBLE, resultBar.getVisibility());
        KeyUtils.singleKeyEventView(getInstrumentation(), findQueryText, KeyEvent.KEYCODE_DEL);
        assertEquals(View.VISIBLE, resultBar.getVisibility());
    }

    /**
     * Verify Find In Page isn't dismissed and matches no results
     * if invoked with an empty string.
     */
    @MediumTest
    @Feature({"FindInPage"})
    public void testFindDismissOnEmptyString() throws InterruptedException {
        loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final TextView findQueryText = getFindQueryText();
        KeyUtils.singleKeyEventView(getInstrumentation(), findQueryText, KeyEvent.KEYCODE_T);
        KeyUtils.singleKeyEventView(getInstrumentation(), findQueryText, KeyEvent.KEYCODE_DEL);
        KeyUtils.singleKeyEventView(getInstrumentation(), findQueryText, KeyEvent.KEYCODE_ENTER);

        assertEquals(View.VISIBLE, findToolbar.getVisibility());

        String findResults = waitForFindResults("");
        assertEquals(0, findResults.length());
    }

    /**
     * Verify FIP in IncognitoTabs.
     */
    @SmallTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFindNextPreviousIncognitoTab() throws InterruptedException {
        String query = "pitts";
        newIncognitoTabFromMenu();
        loadTestAndVerifyFindInPage(query, "1/7");
        // TODO(jaydeepmehta): Verify number of results and match against boxes drawn.
        singleClickView(getActivity().findViewById(R.id.find_next_button));
        waitForFindResults("2/7");
        singleClickView(getActivity().findViewById(R.id.find_prev_button));
        waitForFindResults("1/7");
    }

    /**
     * Verify Find in Page text isnt restored on Incognito Tabs.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testFipTextNotRestoredIncognitoTab() throws InterruptedException {
        newIncognitoTabFromMenu();
        loadTestAndVerifyFindInPage("pitts", "1/7");
        // close the fip
        final View v = getActivity().findViewById(R.id.close_find_button);
        singleClickView(v);
        waitForFindInPageVisibility(false);

        // Reopen and check the text.
        findInPageFromMenu();
        UiUtils.settleDownUI(getInstrumentation());
        // Verify the text content.
        final EditText e = getFindQueryText();
        String myText = e.getText().toString();
        assertTrue("expected empty string : " + myText, myText.isEmpty());
    }

    /**
     * Verify pasted text in the FindQuery text box doesn't retain formatting
     */
    @SmallTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testPastedTextStylingRemoved() throws InterruptedException {
        loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();

        final FindToolbar findToolbar = getFindToolbar();
        assertEquals(View.VISIBLE, findToolbar.getVisibility());
        final EditText findQueryText = getFindQueryText();

        // Setup the clipboard with a selection of stylized text
        ClipboardManager clipboard = (ClipboardManager) (getInstrumentation().getTargetContext())
                .getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newHtmlText("label", "text", "<b>text</b>"));

        // Emulate pasting the text into the find query text box
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                findQueryText.onTextContextMenuItem(android.R.id.paste);
            }
        });

        // Resulting text in the find query box should be unstyled
        final Spannable text = findQueryText.getText();
        final StyleSpan[] spans = text.getSpans(0, text.length(), StyleSpan.class);
        assertEquals(0, spans.length);
    }

    /**
     * Verify Find in page toolbar is not dismissed when device back key is pressed with the
     * presence of IME. First back key should dismiss IME and second back key should dismiss
     * Find in page toolbar.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @FlakyTest(message = "https://crbug.com/673930")
    public void testBackKeyDoesNotDismissFindWhenImeIsPresent() throws InterruptedException {
        loadUrl(mTestServer.getURL(FILEPATH));
        findInPageFromMenu();
        final TextView findQueryText = getFindQueryText();
        KeyUtils.singleKeyEventView(getInstrumentation(), findQueryText, KeyEvent.KEYCODE_A);
        waitForIME(true);
        // IME is present at this moment, so IME will consume BACK key.
        sendKeys(KeyEvent.KEYCODE_BACK);
        waitForIME(false);
        waitForFindInPageVisibility(true);
        sendKeys(KeyEvent.KEYCODE_BACK);
        waitForFindInPageVisibility(false);
    }

    /**
     * Verify Find in page toolbar is dismissed when device back key is pressed when IME
     * is not present. First back key press itself will dismiss Find in page toolbar.
     */
    @MediumTest
    @Feature({"FindInPage"})
    @RetryOnFailure
    public void testBackKeyDismissesFind() throws InterruptedException {
        loadTestAndVerifyFindInPage("pitts", "1/7");
        waitForIME(true);
        // Hide IME by clicking next button from find tool bar.
        singleClickView(getActivity().findViewById(R.id.find_next_button));
        waitForIME(false);
        sendKeys(KeyEvent.KEYCODE_BACK);
        waitForFindInPageVisibility(false);
    }

    private void waitForIME(final boolean imePresent) {
        // Wait for IME to appear.
        CriteriaHelper.pollUiThread(new Criteria("IME is not getting shown!") {
            @Override
            public boolean isSatisfied() {
                return org.chromium.ui.UiUtils.isKeyboardShowing(getActivity(), getFindQueryText())
                        == imePresent;
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
