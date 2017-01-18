// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.Manifest;
import android.app.Dialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.support.test.filters.LargeTest;
import android.test.MoreAsserts;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.concurrent.Callable;

/**
 * Tests for the BluetoothChooserDialog class.
 */
@RetryOnFailure
public class BluetoothChooserDialogTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    /**
     * Works like the BluetoothChooserDialog class, but records calls to native methods instead of
     * calling back to C++.
     */
    static class BluetoothChooserDialogWithFakeNatives extends BluetoothChooserDialog {
        int mFinishedEventType = -1;
        String mFinishedDeviceId;
        int mRestartSearchCount = 0;

        BluetoothChooserDialogWithFakeNatives(WindowAndroid windowAndroid, String origin,
                int securityLevel, long nativeBluetoothChooserDialogPtr) {
            super(windowAndroid, origin, securityLevel, nativeBluetoothChooserDialogPtr);
        }

        @Override
        void nativeOnDialogFinished(
                long nativeBluetoothChooserAndroid, int eventType, String deviceId) {
            assertEquals(nativeBluetoothChooserAndroid, mNativeBluetoothChooserDialogPtr);
            assertEquals(mFinishedEventType, -1);
            mFinishedEventType = eventType;
            mFinishedDeviceId = deviceId;
            // The native code calls closeDialog() when OnDialogFinished is called.
            closeDialog();
        }

        @Override
        void nativeRestartSearch(long nativeBluetoothChooserAndroid) {
            assertTrue(mNativeBluetoothChooserDialogPtr != 0);
            mRestartSearchCount++;
        }

        @Override
        void nativeShowBluetoothOverviewLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            assertTrue(mNativeBluetoothChooserDialogPtr != 0);
        }

        @Override
        void nativeShowBluetoothAdapterOffLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            assertTrue(mNativeBluetoothChooserDialogPtr != 0);
        }

        @Override
        void nativeShowNeedLocationPermissionLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            assertTrue(mNativeBluetoothChooserDialogPtr != 0);
        }
    }

    private ActivityWindowAndroid mWindowAndroid;
    private FakeLocationUtils mLocationUtils;
    private BluetoothChooserDialogWithFakeNatives mChooserDialog;

    public BluetoothChooserDialogTest() {
        super(ChromeActivity.class);
    }

    // ChromeActivityTestCaseBase:

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mLocationUtils = new FakeLocationUtils();
        LocationUtils.setFactory(new LocationUtils.Factory() {
            @Override
            public LocationUtils create() {
                return mLocationUtils;
            }
        });
        mChooserDialog = createDialog();
    }

    @Override
    protected void tearDown() throws Exception {
        LocationUtils.setFactory(null);
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private BluetoothChooserDialogWithFakeNatives createDialog() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<BluetoothChooserDialogWithFakeNatives>() {
                    @Override
                    public BluetoothChooserDialogWithFakeNatives call() {
                        mWindowAndroid = new ActivityWindowAndroid(getActivity());
                        BluetoothChooserDialogWithFakeNatives dialog =
                                new BluetoothChooserDialogWithFakeNatives(mWindowAndroid,
                                        "https://origin.example.com/",
                                        ConnectionSecurityLevel.SECURE, 42);
                        dialog.show();
                        return dialog;
                    }
                });
    }

    private static void selectItem(final BluetoothChooserDialogWithFakeNatives chooserDialog,
            int position) {
        final Dialog dialog = chooserDialog.mItemChooserDialog.getDialogForTesting();
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return items.getChildAt(0) != null;
            }
        });

        assertEquals("Not all items have a view; positions may be incorrect.",
                items.getChildCount(), items.getAdapter().getCount());

        // Verify first item selected gets selected.
        TouchCommon.singleClickView(items.getChildAt(position - 1));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return button.isEnabled();
            }
        });

        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return chooserDialog.mFinishedEventType != -1;
            }
        });
    }

    /**
     * The messages include <*link*> ... </*link*> sections that are used to create clickable spans.
     * For testing the messages, this function returns the raw string without the tags.
     */
    private static String removeLinkTags(String message) {
        return message.replaceAll("</?[^>]*link[^>]*>", "");
    }

    @LargeTest
    public void testCancel() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.GONE, items.getVisibility());

        dialog.dismiss();

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mChooserDialog.mFinishedEventType != -1;
            }
        });

        assertEquals(BluetoothChooserDialog.DIALOG_FINISHED_CANCELLED,
                mChooserDialog.mFinishedEventType);
        assertEquals("", mChooserDialog.mFinishedDeviceId);
    }

    @LargeTest
    public void testSelectItem() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();

        TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChooserDialog.addOrUpdateDevice("id-1", "Name 1");
                mChooserDialog.addOrUpdateDevice("id-2", "Name 2");
            }
        });

        // After adding items to the dialog, the help message should be showing,
        // the progress spinner should disappear, the Commit button should still
        // be disabled (since nothing's selected), and the list view should
        // show.
        assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.VISIBLE, items.getVisibility());
        assertEquals(View.GONE, progress.getVisibility());

        selectItem(mChooserDialog, 2);

        assertEquals(
                BluetoothChooserDialog.DIALOG_FINISHED_SELECTED, mChooserDialog.mFinishedEventType);
        assertEquals("id-2", mChooserDialog.mFinishedDeviceId);
    }

    @LargeTest
    public void testNoLocationPermission() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChooserDialog.notifyDiscoveryState(
                        BluetoothChooserDialog.DISCOVERY_FAILED_TO_START);
            }
        });

        assertEquals(removeLinkTags(
                             getActivity().getString(R.string.bluetooth_need_location_permission)),
                errorView.getText().toString());
        assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_adapter_off_help)),
                statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.VISIBLE, errorView.getVisibility());
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.GONE, progress.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                errorView.getClickableSpans()[0].onClick(errorView);
            }
        });

        // Permission was requested.
        MoreAsserts.assertEquals(permissionDelegate.mPermissionsRequested,
                new String[] {Manifest.permission.ACCESS_COARSE_LOCATION});
        assertNotNull(permissionDelegate.mCallback);
        // Grant permission.
        mLocationUtils.mLocationGranted = true;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                permissionDelegate.mCallback.onRequestPermissionsResult(
                        new String[] {Manifest.permission.ACCESS_COARSE_LOCATION},
                        new int[] {PackageManager.PERMISSION_GRANTED});
            }
        });

        // TODO(661862): Remove once the dialog no longer closes when the window loses
        // focus.
        assertTrue(mChooserDialog.mFinishedEventType != -1);

        // TODO(661862): Uncomment once the dialog no longer closes when the window
        // loses focus.
        // assertEquals(1, mChooserDialog.mRestartSearchCount);
        // assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_searching)),
        //     statusView.getText().toString());
        // mChooserDialog.closeDialog();
    }

    @LargeTest
    public void testNoLocationServices() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        // Grant permissions, and turn off location services.
        mLocationUtils.mLocationGranted = true;
        mLocationUtils.mSystemLocationSettingsEnabled = false;

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChooserDialog.notifyDiscoveryState(
                        BluetoothChooserDialog.DISCOVERY_FAILED_TO_START);
            }
        });

        assertEquals(removeLinkTags(
                             getActivity().getString(R.string.bluetooth_need_location_services_on)),
                errorView.getText().toString());
        assertEquals(removeLinkTags(getActivity().getString(
                             R.string.bluetooth_need_location_permission_help)),
                statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.VISIBLE, errorView.getVisibility());
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.GONE, progress.getVisibility());

        // Turn on Location Services.
        mLocationUtils.mSystemLocationSettingsEnabled = true;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChooserDialog.mLocationModeBroadcastReceiver.onReceive(
                        getActivity(), new Intent(LocationManager.MODE_CHANGED_ACTION));
            }
        });

        assertEquals(1, mChooserDialog.mRestartSearchCount);
        assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_searching)),
                statusView.getText().toString());

        mChooserDialog.closeDialog();
    }

    // TODO(jyasskin): Test when the user denies Chrome the ability to ask for permission.

    @LargeTest
    public void testTurnOnAdapter() {
        final ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        // Turn off adapter.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mChooserDialog.notifyAdapterTurnedOff();
            }
        });

        assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_adapter_off)),
                errorView.getText().toString());
        assertEquals(removeLinkTags(getActivity().getString(R.string.bluetooth_adapter_off_help)),
                statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.VISIBLE, errorView.getVisibility());
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.GONE, progress.getVisibility());

        // Turn on adapter.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                itemChooser.signalInitializingAdapter();
            }
        });

        assertEquals(View.GONE, errorView.getVisibility());
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.VISIBLE, progress.getVisibility());

        mChooserDialog.closeDialog();
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        Dialog mDialog = null;
        PermissionCallback mCallback = null;
        String[] mPermissionsRequested = null;

        public TestAndroidPermissionDelegate(Dialog dialog) {
            mDialog = dialog;
        }

        @Override
        public boolean hasPermission(String permission) {
            return false;
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return true;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
            // Requesting for permission takes away focus from the window.
            mDialog.onWindowFocusChanged(false /* hasFocus */);
            mPermissionsRequested = permissions;
            if (permissions.length == 1
                    && permissions[0].equals(Manifest.permission.ACCESS_COARSE_LOCATION)) {
                mCallback = callback;
            }
        }
    }

    private static class FakeLocationUtils extends LocationUtils {
        public boolean mLocationGranted = false;

        @Override
        public boolean hasAndroidLocationPermission() {
            return mLocationGranted;
        }

        public boolean mSystemLocationSettingsEnabled = true;

        @Override
        public boolean isSystemLocationSettingEnabled() {
            return mSystemLocationSettingsEnabled;
        }
    }
}
