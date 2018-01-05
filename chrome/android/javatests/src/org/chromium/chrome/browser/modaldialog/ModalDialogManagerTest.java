// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.support.test.espresso.Espresso;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for displaying and functioning of modal dialogs on tabs.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ModalDialogManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int MAX_DIALOGS = 3;
    private ChromeTabbedActivity mActivity;
    private ModalDialogManager mManager;
    private ModalDialogView[] mModalDialogViews;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mManager = mActivity.getModalDialogManager();
        mModalDialogViews = new ModalDialogView[MAX_DIALOGS];
        for (int i = 0; i < MAX_DIALOGS; i++) mModalDialogViews[i] = createDialog(i);
    }

    @Test
    @SmallTest
    public void testOneDialog() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show a dialog. The pending list should be empty, and the dialog should be showing.
        // Browser controls should be restricted.
        showDialog(0, ModalDialogManager.TAB_MODAL);
        checkPendingSize(0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Dismiss the dialog by clicking positive button.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(not(hasDescendant(withText("0")))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testTwoDialogs() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show the first dialog.
        // The pending list should be empty, and the dialog should be showing.
        // The tab modal container shouldn't be in the window hierarchy when an app modal dialog is
        // showing.
        showDialog(0, ModalDialogManager.APP_MODAL);
        checkPendingSize(0);
        onView(withText("0")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogManager.APP_MODAL);

        // Show the second dialog. It should be added to the pending list, and the first dialog
        // should still be shown.
        showDialog(1, ModalDialogManager.TAB_MODAL);
        checkPendingSize(1);
        onView(withText("0")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogManager.APP_MODAL);

        // Dismiss the first dialog by clicking cancel. The second dialog should be removed from
        // pending list and shown immediately after.
        onView(withText(R.string.cancel)).perform(click());
        checkPendingSize(0);
        onView(withText("0")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(not(hasDescendant(withText("0"))), hasDescendant(withText("1")))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Dismiss the second dialog by clicking ok. Browser controls should no longer be
        // restricted.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(0);
        onView(withText("0")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(
                        not(hasDescendant(withText("0"))), not(hasDescendant(withText("1"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testThreeDialogs() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Show the first dialog.
        // The pending list should be empty, and the dialog should be showing.
        // Browser controls should be restricted.
        showDialog(0, ModalDialogManager.TAB_MODAL);
        checkPendingSize(0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(hasDescendant(withText("0"))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Show the second dialog. It should be added to the pending list, and the first dialog
        // should still be shown.
        showDialog(1, ModalDialogManager.TAB_MODAL);
        checkPendingSize(1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("0")), not(hasDescendant(withText("1"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Show the third dialog. It should be added to the pending list, and the first dialog
        // should still be shown.
        showDialog(2, ModalDialogManager.APP_MODAL);
        checkPendingSize(2);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Stimulate dismissing the dialog by non-user action. The second dialog should be removed
        // from pending list without showing.
        dismissDialog(1);
        checkPendingSize(1);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Dismiss the second dialog twice and verify nothing breaks.
        dismissDialog(1);
        checkPendingSize(1);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Dismiss the first dialog. The third dialog should be removed from pending list and
        // shown immediately after. The tab modal container shouldn't be in the window hierarchy
        // when an app modal dialog is showing.
        dismissDialog(0);
        checkPendingSize(0);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogManager.APP_MODAL);

        // Dismiss the third dialog by clicking OK. Browser controls should no longer be restricted.
        onView(withText(R.string.ok)).perform(click());
        checkPendingSize(0);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(not(hasDescendant(withText("0"))),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testShow_UrlBarFocused() throws Exception {
        // Show a dialog. The dialog should be shown on top of the toolbar.
        showDialog(0, ModalDialogManager.TAB_MODAL);

        TabModalPresenter presenter =
                (TabModalPresenter) mManager.getPresenterForTest(ModalDialogManager.TAB_MODAL);
        final View dialogContainer = presenter.getDialogContainerForTest();
        final View controlContainer = mActivity.findViewById(R.id.control_container);
        final ViewGroup containerParent = presenter.getContainerParentForTest();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(containerParent.indexOfChild(dialogContainer)
                    > containerParent.indexOfChild(controlContainer));
        });

        // When editing URL, it should be shown on top of the dialog.
        onView(withId(R.id.url_bar)).perform(click());
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(containerParent.indexOfChild(dialogContainer)
                    < containerParent.indexOfChild(controlContainer));
        });

        // When URL bar is not focused, the dialog should be shown on top of the toolbar again.
        Espresso.pressBack();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(containerParent.indexOfChild(dialogContainer)
                    > containerParent.indexOfChild(controlContainer));
        });

        // Dismiss the dialog by clicking OK.
        onView(withText(R.string.ok)).perform(click());
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testDismiss_ToggleOverview() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add two dialogs available for showing.
        showDialog(0, ModalDialogManager.TAB_MODAL);
        showDialog(1, ModalDialogManager.TAB_MODAL);
        checkPendingSize(1);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(
                        allOf(hasDescendant(withText("0")), not(hasDescendant(withText("1"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Dialogs should all be dismissed on entering tab switcher.
        onView(withId(R.id.tab_switcher_button)).perform(click());
        checkPendingSize(0);
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(
                        not(hasDescendant(withText("0"))), not(hasDescendant(withText("1"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    @Test
    @SmallTest
    public void testDismiss_BackPressed() throws Exception {
        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(0);
        checkBrowserControls(false);
        checkCurrentPresenter(null);

        // Add two dialogs available for showing.
        showDialog(0, ModalDialogManager.TAB_MODAL);
        showDialog(1, ModalDialogManager.TAB_MODAL);
        showDialog(2, ModalDialogManager.APP_MODAL);
        checkPendingSize(2);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(hasDescendant(withText("0")),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Perform back press. The first dialog should be dismissed.
        // The second dialog should be shown.
        Espresso.pressBack();
        checkPendingSize(1);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(not(hasDescendant(withText("0"))),
                        hasDescendant(withText("1")), not(hasDescendant(withText("2"))))));
        checkBrowserControls(true);
        checkCurrentPresenter(ModalDialogManager.TAB_MODAL);

        // Perform a second back press. The second dialog should be dismissed.
        // The tab modal container shouldn't be in the window hierarchy when an app modal dialog is
        // showing.
        Espresso.pressBack();
        checkPendingSize(0);
        onView(withText("2")).check(matches(isDisplayed()));
        onView(withId(R.id.tab_modal_dialog_container)).check(doesNotExist());
        checkCurrentPresenter(ModalDialogManager.APP_MODAL);

        // Perform a third back press. The third dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(0);
        onView(withText("2")).check(doesNotExist());
        onView(withId(R.id.tab_modal_dialog_container))
                .check(matches(allOf(not(hasDescendant(withText("0"))),
                        not(hasDescendant(withText("1"))), not(hasDescendant(withText("2"))))));
        checkBrowserControls(false);
        checkCurrentPresenter(null);
    }

    private ModalDialogView createDialog(final int index) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> {
            ModalDialogView.Controller controller = new ModalDialogView.Controller() {
                @Override
                public void onCancel() {}

                @Override
                public void onClick(int buttonType) {
                    switch (buttonType) {
                        case ModalDialogView.BUTTON_POSITIVE:
                        case ModalDialogView.BUTTON_NEGATIVE:
                            dismissDialog(index);
                            break;
                        default:
                            Assert.fail("Unknown button type: " + buttonType);
                    }
                }
            };
            final ModalDialogView.Params p = new ModalDialogView.Params();
            p.title = Integer.toString(index);
            p.positiveButtonTextId = R.string.ok;
            p.negativeButtonTextId = R.string.cancel;
            return new ModalDialogView(controller, p);
        });
    }

    private void showDialog(
            final int index, final @ModalDialogManager.ModalDialogType int dialogType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mManager.showDialog(mModalDialogViews[index], dialogType); });
    }

    private void dismissDialog(final int index) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mManager.dismissDialog(mModalDialogViews[index]); });
    }

    private void checkPendingSize(final int expected) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(expected, mManager.getPendingDialogsForTest().size());
        });
    }

    private void checkCurrentPresenter(final Integer dialogType) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (dialogType == null) {
                Assert.assertFalse(mManager.isShowing());
                Assert.assertNull(mManager.getCurrentPresenterForTest());
            } else {
                Assert.assertTrue(mManager.isShowing());
                Assert.assertEquals(mManager.getPresenterForTest(dialogType),
                        mManager.getCurrentPresenterForTest());
            }
        });
    }

    private void checkBrowserControls(boolean restricted) {
        if (restricted) {
            Assert.assertTrue("All tabs should be obscured", mActivity.isViewObscuringAllTabs());
            onView(withId(R.id.menu_button)).check(matches(not(isEnabled())));
        } else {
            Assert.assertFalse("Tabs shouldn't be obscured", mActivity.isViewObscuringAllTabs());
            onView(withId(R.id.menu_button)).check(matches(isEnabled()));
        }
    }
}
