// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;

import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.content.Context;
import android.support.design.widget.TabLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.PerformException;
import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.ViewInteraction;
import android.view.View;
import android.view.ViewGroup;

import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.InsetObserverView;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.ui.DropdownPopupWindowInterface;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helpers in this class simplify interactions with the Keyboard Accessory and the sheet below it.
 */
public class ManualFillingTestHelper {
    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<>();
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    private class FakeKeyboard extends KeyboardVisibilityDelegate {
        static final int KEYBOARD_HEIGHT = 234;

        private boolean mIsShowing;

        @Override
        public void showKeyboard(View view) {
            mIsShowing = true;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .onKeyboardVisibilityChanged(mIsShowing);
            });
        }

        @Override
        public boolean hideKeyboard(View view) {
            boolean keyboardWasVisible = mIsShowing;
            mIsShowing = false;
            ThreadUtils.runOnUiThreadBlocking(() -> {
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .onKeyboardVisibilityChanged(mIsShowing);
            });
            return keyboardWasVisible;
        }

        @Override
        public int calculateKeyboardHeight(Context context, View rootView) {
            return mIsShowing ? KEYBOARD_HEIGHT : 0;
        }

        @Override
        protected int calculateKeyboardDetectionThreshold(Context context, View rootView) {
            return 0;
        }

        /**
         * Creates an inset observer view calculating the bottom inset based on the fake keyboard.
         * @param context Context used to instantiate this view.
         * @return a {@link InsetObserverView}
         */
        InsetObserverView createInsetObserver(Context context) {
            return new InsetObserverView(context) {
                @Override
                public int getSystemWindowInsetsBottom() {
                    return mIsShowing ? KEYBOARD_HEIGHT : 0;
                }
            };
        }
    }
    private final FakeKeyboard mKeyboard = new FakeKeyboard();

    ManualFillingTestHelper(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    public void loadTestPage(boolean isRtl) throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html"
                + (isRtl ? " dir=\"rtl\"" : "") + "><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"password\" id=\"password\"/><br>"
                + "<input type=\"text\" id=\"email\" autocomplete=\"email\" /><br>"
                + "<input type=\"submit\" id=\"submit\" />"
                + "</form></body></html>"));
        setRtlForTesting(isRtl);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity activity = mActivityTestRule.getActivity();
            mWebContentsRef.set(activity.getActivityTab().getWebContents());
            activity.getManualFillingController()
                    .getMediatorForTesting()
                    .setInsetObserverViewSupplier(() -> {
                        return mKeyboard.createInsetObserver(
                                activity.getInsetObserverView().getContext());
                    });
            // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard is
            // never brought up.
            final ImeAdapter imeAdapter = ImeAdapter.fromWebContents(mWebContentsRef.get());
            mInputMethodManagerWrapper = TestInputMethodManagerWrapper.create(imeAdapter);
            imeAdapter.setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        });
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), "password");
        KeyboardVisibilityDelegate.setDelegateForTesting(mKeyboard); // Use a fake keyboard.
    }
    public void clear() {
        KeyboardVisibilityDelegate.clearDelegateForTesting();
    }

    public void waitForKeyboard() {
        CriteriaHelper.pollUiThread(() -> {
            return mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                    InstrumentationRegistry.getContext(),
                    mActivityTestRule.getActivity().getCurrentFocus());
        });
    }

    public void waitForKeyboardToDisappear() {
        CriteriaHelper.pollUiThread(
                ()
                        -> !KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                                InstrumentationRegistry.getContext(),
                                mActivityTestRule.getActivity().getCurrentFocus()));
    }

    public void clickPasswordField() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "password");
        requestShowKeyboardAccessory();
        mKeyboard.showKeyboard(null);
    }

    public void clickEmailField(boolean forceAccessory)
            throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "email");
        if (forceAccessory) {
            requestShowKeyboardAccessory();
        } else {
            requestHideKeyboardAccessory();
        }
        mKeyboard.showKeyboard(null);
    }

    public DropdownPopupWindowInterface waitForAutofillPopup(String filterInput)
            throws InterruptedException, ExecutionException, TimeoutException {
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();
        final ViewGroup view = webContents.getViewAndroidDelegate().getContainerView();

        // Wait for InputConnection to be ready and fill the filterInput. Then wait for the anchor.
        CriteriaHelper.pollUiThread(
                Criteria.equals(1, () -> mInputMethodManagerWrapper.getShowSoftInputCounter()));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ImeAdapter.fromWebContents(webContents).setComposingTextForTest(filterInput, 4);
        });
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                return view.findViewById(R.id.dropdown_popup_window) != null;
            }
        });
        View anchorView = view.findViewById(R.id.dropdown_popup_window);

        Assert.assertTrue(anchorView.getTag() instanceof DropdownPopupWindowInterface);
        final DropdownPopupWindowInterface popup =
                (DropdownPopupWindowInterface) anchorView.getTag();
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                // Wait until the popup is showing and onLayout() has happened.
                return popup.isShowing() && popup.getListView() != null
                        && popup.getListView().getHeight() != 0;
            }
        });

        return popup;
    }

    /**
     * Although the submit button has no effect, it takes the focus from the input field and should
     * hide the keyboard.
     */
    public void clickSubmit() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "submit");
        mKeyboard.hideKeyboard(null);
    }

    /**
     * Creates and adds a password tab to keyboard accessory and sheet.
     */
    public void createTestTab() {
        KeyboardAccessoryData.Provider<KeyboardAccessoryData.Item> provider =
                new KeyboardAccessoryData.PropertyProvider<>();
        mActivityTestRule.getActivity().getManualFillingController().registerPasswordProvider(
                provider);
        provider.notifyObservers(new KeyboardAccessoryData.Item[] {
                KeyboardAccessoryData.Item.createSuggestion("TestName", "", false, null, null),
                KeyboardAccessoryData.Item.createSuggestion(
                        "TestPassword", "", false, (item) -> {}, null)});
    }

    /**
     * Use in a |onView().perform| action to select the tab at |tabIndex| for the found tab layout.
     * @param tabIndex The index to be selected.
     * @return The action executed by |perform|.
     */
    static public ViewAction selectTabAtPosition(int tabIndex) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(isDisplayed(), isAssignableFrom(TabLayout.class));
            }

            @Override
            public String getDescription() {
                return "with tab at index " + tabIndex;
            }

            @Override
            public void perform(UiController uiController, View view) {
                TabLayout tabLayout = (TabLayout) view;
                if (tabLayout.getTabAt(tabIndex) == null) {
                    throw new PerformException.Builder()
                            .withCause(new Throwable("No tab at index " + tabIndex))
                            .build();
                }
                tabLayout.getTabAt(tabIndex).select();
            }
        };
    }

    /**
     * Use like {@link android.support.test.espresso.Espresso#onView}. It waits for a view matching
     * the given |matcher| to be displayed and allows to chain checks/performs on the result.
     * @param matcher The matcher matching exactly the view that is expected to be displayed.
     * @return An interaction on the view matching |matcher.
     */
    public static ViewInteraction whenDisplayed(Matcher<View> matcher) {
        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, allOf(matcher, isDisplayed())));
        return onView(matcher);
    }

    public void waitToBeHidden(Matcher<View> matcher) {
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r, matcher, VIEW_INVISIBLE | VIEW_NULL | VIEW_GONE);
        });
    }

    /**
     * In order to make sure the keyboard accessory is only shown on appropriate fields, a request
     * to show is usually sent from the native side. This method simulates that request.
     */
    private void requestShowKeyboardAccessory() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getManualFillingController()
                    .getMediatorForTesting()
                    .showWhenKeyboardIsVisible();
        });
    }

    /**
     * In order to make sure the keyboard accessory is only shown on appropriate fields, a request
     * from the native side can request to hide it. This method simulates that request.
     */
    private void requestHideKeyboardAccessory() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getManualFillingController()
                    .getMediatorForTesting()
                    .hide();
        });
    }
}
