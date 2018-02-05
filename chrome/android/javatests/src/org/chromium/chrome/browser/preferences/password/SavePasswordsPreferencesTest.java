// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import static android.support.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.closeSoftKeyboard;
import static android.support.test.espresso.action.ViewActions.scrollTo;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.intent.Intents.intended;
import static android.support.test.espresso.intent.Intents.intending;
import static android.support.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasType;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.startsWith;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.PerformException;
import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.intent.Intents;
import android.support.test.espresso.util.TreeIterables;
import android.support.test.filters.SmallTest;
import android.view.MenuItem;
import android.view.View;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the "Save Passwords" settings screen.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SavePasswordsPreferencesTest {
    private static final long UI_UPDATING_TIMEOUT_MS = 3000;
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private static final class FakePasswordManagerHandler implements PasswordManagerHandler {
        // This class has exactly one observer, set on construction and expected to last at least as
        // long as this object (a good candidate is the owner of this object).
        private final PasswordListObserver mObserver;

        // The faked contents of the password store to be displayed.
        private ArrayList<SavedPasswordEntry> mSavedPasswords = new ArrayList<SavedPasswordEntry>();

        // The faked contents of the saves password exceptions to be displayed.
        private ArrayList<String> mSavedPasswordExeptions = new ArrayList<>();

        // This is set once {@link #serializePasswords()} is called.
        @Nullable
        private Callback<String> mExportCallback;

        public void setSavedPasswords(ArrayList<SavedPasswordEntry> savedPasswords) {
            mSavedPasswords = savedPasswords;
        }

        public void setSavedPasswordExceptions(ArrayList<String> savedPasswordExceptions) {
            mSavedPasswordExeptions = savedPasswordExceptions;
        }

        public Callback<String> getExportCallback() {
            return mExportCallback;
        }

        /**
         * Constructor.
         * @param PasswordListObserver The only observer.
         */
        public FakePasswordManagerHandler(PasswordListObserver observer) {
            mObserver = observer;
        }

        // Pretends that the updated lists are |mSavedPasswords| for the saved passwords and an
        // empty list for exceptions and immediately calls the observer.
        @Override
        public void updatePasswordLists() {
            mObserver.passwordListAvailable(mSavedPasswords.size());
            mObserver.passwordExceptionListAvailable(mSavedPasswordExeptions.size());
        }

        @Override
        public SavedPasswordEntry getSavedPasswordEntry(int index) {
            return mSavedPasswords.get(index);
        }

        @Override
        public String getSavedPasswordException(int index) {
            return mSavedPasswordExeptions.get(index);
        }

        @Override
        public void removeSavedPasswordEntry(int index) {
            // Define this method before starting to use it in tests.
            assert false;
            return;
        }

        @Override
        public void removeSavedPasswordException(int index) {
            // Define this method before starting to use it in tests.
            assert false;
            return;
        }

        @Override
        public void serializePasswords(Callback<String> callback) {
            mExportCallback = callback;
        }
    }

    private final static SavedPasswordEntry ZEUS_ON_EARTH =
            new SavedPasswordEntry("http://www.phoenicia.gr", "Zeus", "Europa");
    private final static SavedPasswordEntry ARES_AT_OLYMP =
            new SavedPasswordEntry("https://1-of-12.olymp.gr", "Ares", "God-o'w@r");
    private final static SavedPasswordEntry PHOBOS_AT_OLYMP =
            new SavedPasswordEntry("https://visitor.olymp.gr", "Phobos-son-of-ares", "G0d0fF34r");
    private final static SavedPasswordEntry DEIMOS_AT_OLYMP =
            new SavedPasswordEntry("https://visitor.olymp.gr", "Deimops-Ares-son", "G0d0fT3rr0r");
    private final static SavedPasswordEntry HADES_AT_UNDERWORLD =
            new SavedPasswordEntry("https://underworld.gr", "", "C3rb3rus");
    private final static SavedPasswordEntry[] GREEK_GODS = {
            ZEUS_ON_EARTH, ARES_AT_OLYMP, PHOBOS_AT_OLYMP, DEIMOS_AT_OLYMP, HADES_AT_UNDERWORLD,
    };

    // Used to provide fake lists of stored passwords. Tests which need it can use setPasswordSource
    // to instantiate it.
    FakePasswordManagerHandler mHandler;

    /**
     * Helper to set up a fake source of displayed passwords.
     * @param entry An entry to be added to saved passwords. Can be null.
     */
    private void setPasswordSource(SavedPasswordEntry entry) throws Exception {
        SavedPasswordEntry[] entries = {};
        if (entry != null) {
            entries = new SavedPasswordEntry[] {entry};
        }
        setPasswordSourceWithMultipleEntries(entries);
    }

    /**
     * Helper to set up a fake source of displayed passwords with multiple initial passwords.
     * @param initialEntries All entries to be added to saved passwords. Can not be null.
     */
    private void setPasswordSourceWithMultipleEntries(SavedPasswordEntry[] initialEntries)
            throws Exception {
        if (mHandler == null) {
            mHandler = new FakePasswordManagerHandler(PasswordManagerHandlerProvider.getInstance());
        }
        ArrayList<SavedPasswordEntry> entries = new ArrayList<>(Arrays.asList(initialEntries));
        mHandler.setSavedPasswords(entries);
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordManagerHandlerProvider.getInstance()
                                   .setPasswordManagerHandlerForTest(mHandler));
    }

    /**
     * Helper to set up a fake source of displayed passwords without passwords but with exceptions.
     * @param exceptions All exceptions to be added to saved exceptions. Can not be null.
     */
    private void setPasswordExceptions(String[] exceptions) throws Exception {
        if (mHandler == null) {
            mHandler = new FakePasswordManagerHandler(PasswordManagerHandlerProvider.getInstance());
        }
        mHandler.setSavedPasswordExceptions(new ArrayList<>(Arrays.asList(exceptions)));
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordManagerHandlerProvider.getInstance()
                                   .setPasswordManagerHandlerForTest(mHandler));
    }

    /**
     * Looks for the search icon by id. If it cannot be found, it's probably hidden in the overflow
     * menu. In that case, open the menu and search for its title.
     * @return Returns either the search icon button or the search menu option.
     */
    public static Matcher<View> withSearchMenuIdOrText() {
        Matcher<View> matcher = withId(R.id.menu_id_search);
        try {
            Espresso.onView(matcher).check(matches(isDisplayed()));
            return matcher;
        } catch (Exception NoMatchingViewException) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            return withText(R.string.search);
        }
    }

    /**
     * Taps the menu item to trigger exporting and ensures that reauthentication passes.
     */
    private void reauthenticateAndRequestExport() {
        Espresso.openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // Before exporting, pretend that the last successful reauthentication just
        // happened. This will allow the export flow to continue.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.REAUTH_SCOPE_BULK);
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());
    }

    /**
     * Call after activity.finish() to wait for the wrap up to complete. If it was already completed
     * or could be finished within |timeout_ms|, stop waiting anyways.
     * @param activity The activity to wait for.
     * @param timeout The timeout in ms after which the waiting will end anyways.
     * @throws InterruptedException
     */
    private void waitToFinish(Activity activity, long timeout) throws InterruptedException {
        long start_time = System.currentTimeMillis();
        while (activity.isFinishing() && (System.currentTimeMillis() - start_time < timeout))
            Thread.sleep(100);
    }

    /**
     * Waits until a view matching the given matcher appears. Times out if no view was found until
     * the UI_UPDATING_TIMEOUT_MS expired.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    private void waitForView(Matcher<View> viewMatcher) {
        Espresso.onView(isRoot()).perform(new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return isRoot();
            }

            @Override
            public String getDescription() {
                return "wait for " + UI_UPDATING_TIMEOUT_MS + "ms to match "
                        + viewMatcher.toString();
            }

            @Override
            public void perform(UiController uiController, View root) {
                final long start_time = System.currentTimeMillis();
                do {
                    for (View view : TreeIterables.breadthFirstViewTraversal(root))
                        if (viewMatcher.matches(view)) return;
                    uiController.loopMainThreadForAtLeast(100);
                } while (System.currentTimeMillis() - start_time < UI_UPDATING_TIMEOUT_MS);
                throw new PerformException.Builder()
                        .withActionDescription(this.getDescription())
                        .withCause(new TimeoutException())
                        .build();
            }
        });
    }

    /**
     * Ensure that resetting of empty passwords list works.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetListEmpty() throws Exception {
        // Load the preferences, they should show the empty list.
        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences savePasswordPreferences =
                        (SavePasswordsPreferences) preferences.getFragmentForTest();
                // Emulate an update from PasswordStore. This should not crash.
                savePasswordPreferences.passwordListAvailable(0);
            }
        });
    }

    /**
     * Ensure that the on/off switch in "Save Passwords" settings actually enables and disables
     * password saving.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSavePasswordsSwitch() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setRememberPasswordsEnabled(true);
            }
        });

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences savedPasswordPrefs =
                        (SavePasswordsPreferences) preferences.getFragmentForTest();
                ChromeSwitchPreference onOffSwitch = (ChromeSwitchPreference)
                        savedPasswordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_SAVE_PASSWORDS_SWITCH);
                Assert.assertTrue(onOffSwitch.isChecked());

                PreferencesTest.clickPreference(savedPasswordPrefs, onOffSwitch);
                Assert.assertFalse(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());
                PreferencesTest.clickPreference(savedPasswordPrefs, onOffSwitch);
                Assert.assertTrue(PrefServiceBridge.getInstance().isRememberPasswordsEnabled());

                preferences.finish();

                PrefServiceBridge.getInstance().setRememberPasswordsEnabled(false);
            }
        });

        final Preferences preferences2 =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences savedPasswordPrefs =
                        (SavePasswordsPreferences) preferences2.getFragmentForTest();
                ChromeSwitchPreference onOffSwitch = (ChromeSwitchPreference)
                        savedPasswordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_SAVE_PASSWORDS_SWITCH);
                Assert.assertFalse(onOffSwitch.isChecked());
            }
        });
    }

    /**
     * Ensure that the "Auto Sign-in" switch in "Save Passwords" settings actually enables and
     * disables auto sign-in.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutoSignInCheckbox() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(true);
            }
        });

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences passwordPrefs =
                        (SavePasswordsPreferences) preferences.getFragmentForTest();
                ChromeBaseCheckBoxPreference onOffSwitch =
                        (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_AUTOSIGNIN_SWITCH);
                Assert.assertTrue(onOffSwitch.isChecked());

                PreferencesTest.clickPreference(passwordPrefs, onOffSwitch);
                Assert.assertFalse(
                        PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());
                PreferencesTest.clickPreference(passwordPrefs, onOffSwitch);
                Assert.assertTrue(
                        PrefServiceBridge.getInstance().isPasswordManagerAutoSigninEnabled());

                preferences.finish();

                PrefServiceBridge.getInstance().setPasswordManagerAutoSigninEnabled(false);
            }
        });

        final Preferences preferences2 =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SavePasswordsPreferences passwordPrefs =
                        (SavePasswordsPreferences) preferences2.getFragmentForTest();
                ChromeBaseCheckBoxPreference onOffSwitch =
                        (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                                SavePasswordsPreferences.PREF_AUTOSIGNIN_SWITCH);
                Assert.assertFalse(onOffSwitch.isChecked());
            }
        });
    }

    /**
     * Check that if there are no saved passwords, the export menu item is disabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportMenuDisabled() throws Exception {
        // Ensure there are no saved passwords reported to settings.
        setPasswordSource(null);

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // The text matches a text view, but the disabled entity is some wrapper two levels up in
        // the view hierarchy, hence the two withParent matchers.
        Espresso.onView(allOf(withText(R.string.save_password_preferences_export_action_title),
                                withParent(withParent(not(isEnabled())))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that if there are saved passwords, the export menu item is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportMenuEnabled() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // The text matches a text view, but the potentially disabled entity is some wrapper two
        // levels up in the view hierarchy, hence the two withParent matchers.
        Espresso.onView(allOf(withText(R.string.save_password_preferences_export_action_title),
                                withParent(withParent(isEnabled()))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that if "PasswordExport" feature is not explicitly enabled, there is no menu item to
     * export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures("PasswordExport")
    public void testExportMenuMissing() throws Exception {
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        // Ideally this would need the same matcher (Espresso.OVERFLOW_BUTTON_MATCHER) as used
        // inside Espresso.openActionBarOverflowOrOptionsMenu(), but that is private to Espresso.
        // Matching the overflow menu with the class name "OverflowMenuButton" won't work on
        // obfuscated release builds, so matching the description remains. The
        // OVERFLOW_BUTTON_MATCHER specifies the string directly, not via string resource, so this
        // is also done below.
        Espresso.onView(withContentDescription("More options")).check(doesNotExist());
    }

    /**
     * Check that tapping the export menu requests the passwords to be serialised in the background.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportTriggersSerialization() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // Before tapping the menu item for export, pretend that the last successful
        // reauthentication just happened. This will allow the export flow to continue.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.REAUTH_SCOPE_BULK);
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());

        Assert.assertTrue(mHandler.getExportCallback() != null);
    }

    /**
     * Check that the export menu item is included and hidden behind the overflow menu. Check that
     * the menu displays the warning before letting the user export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportMenuItem() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // Before tapping the menu item for export, pretend that the last successful
        // reauthentication just happened. This will allow the export flow to continue.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.REAUTH_SCOPE_BULK);
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());

        Espresso.onView(withText(R.string.settings_passwords_export_description))
                .check(matches(isDisplayed()));
    }

    /**
     * Check whether the user is asked to set up a screen lock if attempting to export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportMenuItemNoLock() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_UNAVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        View mainDecorView = preferences.getWindow().getDecorView();
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());
        Espresso.onView(withText(R.string.password_export_set_lock_screen))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that if exporting is cancelled for the absence of the screen lock, the menu item is
     * enabled for a retry.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportMenuItemReenabledNoLock() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_UNAVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // The text matches a text view, but the potentially disabled entity is some wrapper two
        // levels up in the view hierarchy, hence the two withParent matchers.
        Espresso.onView(allOf(withText(R.string.save_password_preferences_export_action_title),
                                withParent(withParent(isEnabled()))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that if exporting is cancelled for the user's failure to reauthenticate, the menu item
     * is enabled for a retry.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportMenuItemReenabledReauthFailure() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setSkipSystemReauth(true);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());
        // The reauthentication dialog is skipped and the last reauthentication timestamp is not
        // reset. This looks like a failed reauthentication to SavePasswordsPreferences' onResume.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                preferences.getFragmentForTest().onResume();
            }
        });
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // The text matches a text view, but the potentially disabled entity is some wrapper two
        // levels up in the view hierarchy, hence the two withParent matchers.
        Espresso.onView(allOf(withText(R.string.save_password_preferences_export_action_title),
                                withParent(withParent(isEnabled()))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that the export flow ends up with sending off a share intent with the exported
     * passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportIntent() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        Intents.init();

        reauthenticateAndRequestExport();

        // Pretend that passwords have been serialized to go directly to the intent.
        mHandler.getExportCallback().onResult("serialized passwords");

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/csv"))))));

        Intents.release();
    }

    /**
     * Check that a progressbar is displayed when the user confirms the export and the serialized
     * passwords are not ready yet.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportProgress() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        Intents.init();

        reauthenticateAndRequestExport();

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());

        // Before simulating the serialized passwords being received, check that the progress bar is
        // shown.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(matches(isDisplayed()));

        // Now pretend that passwords have been serialized.
        mHandler.getExportCallback().onResult("serialized passwords");

        // Before simulating the serialized passwords being received, check that the progress bar is
        // hidden.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(doesNotExist());

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/csv"))))));

        Intents.release();
    }

    /**
     * Check that the user can cancel exporting with the "Cancel" button on the progressbar.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("PasswordExport")
    public void testExportCancelOnProgress() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        reauthenticateAndRequestExport();

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.save_password_preferences_export_action_title))
                .perform(click());

        // Check that the progress bar is shown.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(matches(isDisplayed()));

        // Hit the Cancel button.
        Espresso.onView(withText(R.string.cancel)).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // The text matches a text view, but the potentially disabled entity is some wrapper two
        // levels up in the view hierarchy, hence the two withParent matchers.
        Espresso.onView(allOf(withText(R.string.save_password_preferences_export_action_title),
                                withParent(withParent(isEnabled()))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check whether the user is asked to set up a screen lock if attempting to view passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testViewPasswordNoLock() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_UNAVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        View mainDecorView = preferences.getWindow().getDecorView();
        Espresso.onView(withText(containsString("test user"))).perform(click());
        Espresso.onView(withContentDescription(R.string.password_entry_editor_copy_stored_password))
                .perform(click());
        Espresso.onView(withText(R.string.password_entry_editor_set_lock_screen))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check whether the user can view a saved password.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testViewPassword() throws Exception {
        setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "test password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);

        final Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        Espresso.onView(withText(containsString("test user"))).perform(click());

        // Before tapping the view button, pretend that the last successful reauthentication just
        // happened. This will allow showing the password.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.REAUTH_SCOPE_ONE_AT_A_TIME);
        Espresso.onView(withContentDescription(R.string.password_entry_editor_view_stored_password))
                .perform(click());
        Espresso.onView(withText("test password")).check(matches(isDisplayed()));
    }

    /**
     * Check that the search item is visible if the Feature is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    @SuppressWarnings("AlwaysShowAction") // We need to ensure the icon is in the action bar.
    public void testSearchIconVisibleInActionBarWithFeature() throws Exception {
        setPasswordSource(null); // Initialize empty preferences.
        SavePasswordsPreferences f =
                (SavePasswordsPreferences) PreferencesTest
                        .startPreferences(InstrumentationRegistry.getInstrumentation(),
                                SavePasswordsPreferences.class.getName())
                        .getFragmentForTest();

        // Force the search option into the action bar.
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> f.getMenuForTesting()
                                   .findItem(R.id.menu_id_search)
                                   .setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS));

        Espresso.onView(withId(R.id.menu_id_search)).check(matches(isDisplayed()));
    }

    /**
     * Check that the search item is visible if the Feature is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchTextInOverflowMenuVisibleWithFeature() throws Exception {
        setPasswordSource(null); // Initialize empty preferences.
        SavePasswordsPreferences f =
                (SavePasswordsPreferences) PreferencesTest
                        .startPreferences(InstrumentationRegistry.getInstrumentation(),
                                SavePasswordsPreferences.class.getName())
                        .getFragmentForTest();

        // Force the search option into the overflow menu.
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> f.getMenuForTesting()
                                   .findItem(R.id.menu_id_search)
                                   .setShowAsAction(MenuItem.SHOW_AS_ACTION_NEVER));

        // Open the overflow menu.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        Espresso.onView(withText(R.string.search)).check(matches(isDisplayed()));
    }

    /**
     * Check that the search item is not visible if the Feature is disabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchIconGoneWithoutFeature() throws Exception {
        setPasswordSource(null); // Initialize empty preferences.
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        Espresso.onView(withId(R.id.menu_id_search)).check(doesNotExist());
    }

    /**
     * Check that the search filters the list by name.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchFiltersByUserName() throws Exception {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        // Search for a string matching multiple user names. Case doesn't need to match.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("aREs"), closeSoftKeyboard());

        Espresso.onView(withText(ARES_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(DEIMOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
    }

    /**
     * Check that the search filters the list by URL.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchFiltersByUrl() throws Exception {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        // Search for a string that matches multiple URLs. Case doesn't need to match.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Olymp"), closeSoftKeyboard());

        Espresso.onView(withText(ARES_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(DEIMOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
    }

    /**
     * Check that the search filters the list by URL.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchDisplaysBlankPageIfSearchTurnsUpEmpty() throws Exception {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());
        Espresso.onView(withText(startsWith("View and manage"))).check(matches(isDisplayed()));

        // Open the search which should hide the Account link.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());

        // Search for a string that matches nothing which should leave the results entirely blank.
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Mars"), closeSoftKeyboard());

        for (SavedPasswordEntry god : GREEK_GODS) {
            Espresso.onView(allOf(withText(god.getUserName()), withText(god.getUrl())))
                    .check(doesNotExist());
        }
        Espresso.onView(withText(startsWith("View and manage"))).check(doesNotExist());
        Espresso.onView(withText(R.string.saved_passwords_none_text)).check(doesNotExist());
        Espresso.onView(withText(R.string.section_saved_passwords)).check(doesNotExist());
    }

    /**
     * Check that triggering the search hides all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchIconClickedHidesExceptionsTemporarily() throws Exception {
        setPasswordExceptions(new String[] {"http://exclu.de", "http://not-inclu.de"});
        final SavePasswordsPreferences savePasswordPreferences =
                (SavePasswordsPreferences) PreferencesTest
                        .startPreferences(InstrumentationRegistry.getInstrumentation(),
                                SavePasswordsPreferences.class.getName())
                        .getFragmentForTest();

        Espresso.onView(withText(R.string.section_saved_passwords_exceptions)).perform(scrollTo());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Espresso.onView(withText(R.string.section_saved_passwords_exceptions))
                .check(matches(isDisplayed()));

        Espresso.onView(withSearchMenuIdOrText()).perform(click());

        Espresso.onView(withText(R.string.section_saved_passwords_exceptions))
                .check(doesNotExist());

        Espresso.pressBack(); // Close keyboard.
        Espresso.pressBack(); // Close search view.

        Espresso.onView(withText(R.string.section_saved_passwords_exceptions)).perform(scrollTo());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Espresso.onView(withText(R.string.section_saved_passwords_exceptions))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that triggering the search hides all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchIconClickedHidesGeneralPrefs() throws Exception {
        setPasswordSource(ZEUS_ON_EARTH);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        Espresso.onView(withText(R.string.passwords_auto_signin_title))
                .check(matches(isDisplayed()));
        Espresso.onView(withText(startsWith("View and manage"))).check(matches(isDisplayed()));

        Espresso.onView(withSearchMenuIdOrText()).perform(click());

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(startsWith("View and manage"))).check(doesNotExist());
    }

    /**
     * Check that closing the search via back button brings back all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchBarBackButtonRestoresGeneralPrefs() throws Exception {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(startsWith("View and manage"))).check(doesNotExist());

        Espresso.pressBack(); // Close keyboard.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Espresso.onView(withContentDescription("Collapse")).perform(click());

        Espresso.onView(withText(R.string.passwords_auto_signin_title))
                .check(matches(isDisplayed()));
        Espresso.onView(withText(startsWith("View and manage"))).check(matches(isDisplayed()));
    }

    /**
     * Check that closing the search via back button brings back all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchBarBackKeyRestoresGeneralPrefs() throws Exception {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(startsWith("View and manage"))).check(doesNotExist());

        Espresso.pressBack(); // Close keyboard.
        Espresso.pressBack(); // Close search view.

        Espresso.onView(withText(R.string.passwords_auto_signin_title))
                .check(matches(isDisplayed()));
        Espresso.onView(withText(startsWith("View and manage"))).check(matches(isDisplayed()));
    }

    /**
     * Check that the filtered password list persists after the user had inspected a single result.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchResultsPersistAfterEntryInspection() throws Exception {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        setPasswordExceptions(new String[] {"http://exclu.de", "http://not-inclu.de"});
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OVERRIDE_STATE_AVAILABLE);
        PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                SavePasswordsPreferences.class.getName());

        // Open the search and filter all but "Zeus".
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        waitForView(withId(R.id.search_src_text));
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());

        // Click "Zeus" to open edit field and verify the password. Pretend the user just passed the
        // reauthentication challenge.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.REAUTH_SCOPE_ONE_AT_A_TIME);
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Intent.ACTION_VIEW), null, false);
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).perform(click());
        monitor.waitForActivityWithTimeout(UI_UPDATING_TIMEOUT_MS);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        Espresso.onView(withContentDescription(R.string.password_entry_editor_view_stored_password))
                .perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(ZEUS_ON_EARTH.getPassword())).check(matches(isDisplayed()));
        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click()); // Go back to the search list.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // The search bar should still be open and still display the search query.
        waitForView(withId(R.id.search_src_text));
        Espresso.onView(withId(R.id.search_src_text)).check(matches(isDisplayed()));
        Espresso.onView(withId(R.id.search_src_text)).check(matches(withText("Zeu")));
    }

    /**
     * Check that triggering searches and inspected search results are recorded in histograms.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.PASSWORD_SEARCH)
    public void testSearchIsRecordedInHistograms() throws Exception {
        MetricsUtils.HistogramDelta triggered_delta = new MetricsUtils.HistogramDelta(
                "PasswordManager.Android.PasswordSearchTriggered", 1);
        MetricsUtils.HistogramDelta untriggered_delta = new MetricsUtils.HistogramDelta(
                "PasswordManager.Android.PasswordSearchTriggered", 0);
        MetricsUtils.HistogramDelta viewed_after_search_delta = new MetricsUtils.HistogramDelta(
                "PasswordManager.Android.PasswordCredentialEntry", 3);
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SavePasswordsPreferences.class.getName());

        // Open the search and filter all but "Zeus".
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Intent.ACTION_VIEW), null, false);

        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName()))
                .check(matches(isDisplayed()))
                .perform(click());
        monitor.waitForActivityWithTimeout(UI_UPDATING_TIMEOUT_MS);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click()); // Go back to the search list.
        waitForView(withId(R.id.search_src_text));
        Assert.assertEquals(1, viewed_after_search_delta.getDelta());

        preferences.finish();
        waitToFinish(preferences, UI_UPDATING_TIMEOUT_MS);

        Assert.assertEquals(0, untriggered_delta.getDelta());
        Assert.assertEquals(1, triggered_delta.getDelta());
    }
}
