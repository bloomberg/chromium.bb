// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.text.SpannableString;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.ArrayList;
import java.util.List;

/**
 * A dialog for picking available Bluetooth devices. This dialog is shown when a website requests to
 * pair with a certain class of Bluetooth devices (e.g. through a bluetooth.requestDevice Javascript
 * call).
 */
public class BluetoothChooserDialog
        implements ItemChooserDialog.ItemSelectedCallback, WindowAndroid.PermissionCallback {
    // These constants match BluetoothChooserAndroid::ShowDiscoveryState, and are used in
    // notifyDiscoveryState().
    private static final int DISCOVERY_FAILED_TO_START = 0;
    private static final int DISCOVERING = 1;
    private static final int DISCOVERY_IDLE = 2;

    // Values passed to nativeOnDialogFinished:eventType, and only used in the native function.
    private static final int DIALOG_FINISHED_DENIED_PERMISSION = 0;
    private static final int DIALOG_FINISHED_CANCELLED = 1;
    private static final int DIALOG_FINISHED_SELECTED = 2;

    // The window that owns this dialog.
    final WindowAndroid mWindowAndroid;

    // Always equal to mWindowAndroid.getActivity().get(), but stored separately to make sure it's
    // not GC'ed.
    final Context mContext;

    // The dialog to show to let the user pick a device.
    ItemChooserDialog mItemChooserDialog;

    // The origin for the site wanting to pair with the bluetooth devices.
    String mOrigin;

    // The security level of the connection to the site wanting to pair with the
    // bluetooth devices. For valid values see SecurityStateModel::SecurityLevel.
    int mSecurityLevel;

    // A pointer back to the native part of the implementation for this dialog.
    long mNativeBluetoothChooserDialogPtr;

    // The type of link that is shown within the dialog.
    private enum LinkType {
        EXPLAIN_BLUETOOTH,
        ADAPTER_OFF,
        ADAPTER_OFF_HELP,
        REQUEST_LOCATION_PERMISSION,
        NEED_LOCATION_PERMISSION_HELP,
        RESTART_SEARCH,
    }

    /**
     * Creates the BluetoothChooserDialog and displays it (and starts waiting for data).
     *
     * @param context Context which is used for launching a dialog.
     */
    private BluetoothChooserDialog(WindowAndroid windowAndroid, String origin, int securityLevel,
            long nativeBluetoothChooserDialogPtr) {
        mWindowAndroid = windowAndroid;
        mContext = windowAndroid.getActivity().get();
        assert mContext != null;
        mOrigin = origin;
        mSecurityLevel = securityLevel;
        mNativeBluetoothChooserDialogPtr = nativeBluetoothChooserDialogPtr;
    }

    /**
     * Show the BluetoothChooserDialog.
     */
    private void show() {
        // Emphasize the origin.
        Profile profile = Profile.getLastUsedProfile();
        SpannableString origin = new SpannableString(mOrigin);
        OmniboxUrlEmphasizer.emphasizeUrl(
                origin, mContext.getResources(), profile, mSecurityLevel, false, true, true);
        // Construct a full string and replace the origin text with emphasized version.
        SpannableString title =
                new SpannableString(mContext.getString(R.string.bluetooth_dialog_title, mOrigin));
        int start = title.toString().indexOf(mOrigin);
        TextUtils.copySpansFrom(origin, 0, origin.length(), Object.class, title, start);

        String message = mContext.getString(R.string.bluetooth_not_found);
        SpannableString noneFound = SpanApplier.applySpans(
                message, new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(LinkType.RESTART_SEARCH, mContext)));

        SpannableString searching = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_searching),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(LinkType.EXPLAIN_BLUETOOTH, mContext)));

        String positiveButton = mContext.getString(R.string.bluetooth_confirm_button);

        SpannableString statusActive = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_not_seeing_it),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(LinkType.EXPLAIN_BLUETOOTH, mContext)));

        SpannableString statusIdleNoneFound = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_not_seeing_it_idle_none_found),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(LinkType.EXPLAIN_BLUETOOTH, mContext)));

        SpannableString statusIdleSomeFound = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_not_seeing_it_idle_some_found),
                new SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(LinkType.EXPLAIN_BLUETOOTH, mContext)),
                new SpanInfo("<link2>", "</link2>",
                        new NoUnderlineClickableSpan(LinkType.RESTART_SEARCH, mContext)));

        ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(title, searching, noneFound, statusActive,
                        statusIdleNoneFound, statusIdleSomeFound, positiveButton);
        mItemChooserDialog = new ItemChooserDialog(mContext, this, labels);
    }

    @Override
    public void onItemSelected(String id) {
        if (mNativeBluetoothChooserDialogPtr != 0) {
            if (id.isEmpty()) {
                nativeOnDialogFinished(
                        mNativeBluetoothChooserDialogPtr, DIALOG_FINISHED_CANCELLED, "");
            } else {
                nativeOnDialogFinished(
                        mNativeBluetoothChooserDialogPtr, DIALOG_FINISHED_SELECTED, id);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        for (int i = 0; i < grantResults.length; i++) {
            if (permissions[i].equals(Manifest.permission.ACCESS_COARSE_LOCATION)) {
                if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                    mItemChooserDialog.clear();
                    nativeRestartSearch(mNativeBluetoothChooserDialogPtr);
                } else {
                    checkLocationPermission();
                }
                return;
            }
        }
        // If the location permission is not present, leave the currently-shown message in place.
    }

    private void checkLocationPermission() {
        if (mWindowAndroid.hasPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                || mWindowAndroid.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION)) {
            return;
        }

        if (!mWindowAndroid.canRequestPermission(Manifest.permission.ACCESS_COARSE_LOCATION)) {
            if (mNativeBluetoothChooserDialogPtr != 0) {
                nativeOnDialogFinished(
                        mNativeBluetoothChooserDialogPtr, DIALOG_FINISHED_DENIED_PERMISSION, "");
                return;
            }
        }

        SpannableString needLocationMessage = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_need_location_permission),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(
                                     LinkType.REQUEST_LOCATION_PERMISSION, mContext)));

        SpannableString needLocationStatus = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_need_location_permission_help),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(
                                     LinkType.NEED_LOCATION_PERMISSION_HELP, mContext)));

        mItemChooserDialog.setErrorState(needLocationMessage, needLocationStatus);
    }

    /**
     * A helper class to show a clickable link with underlines turned off.
     */
    private class NoUnderlineClickableSpan extends ClickableSpan {
        // The type of link this span represents.
        private LinkType mLinkType;

        private Context mContext;

        NoUnderlineClickableSpan(LinkType linkType, Context context) {
            mLinkType = linkType;
            mContext = context;
        }

        @Override
        public void onClick(View view) {
            if (mNativeBluetoothChooserDialogPtr == 0) {
                return;
            }

            switch (mLinkType) {
                case EXPLAIN_BLUETOOTH: {
                    // No need to close the dialog here because
                    // ShowBluetoothOverviewLink will close it.
                    // TODO(ortuno): The BluetoothChooserDialog should dismiss
                    // itself when a new tab is opened or the current tab navigates.
                    // https://crbug.com/588127
                    nativeShowBluetoothOverviewLink(mNativeBluetoothChooserDialogPtr);
                    break;
                }
                case ADAPTER_OFF: {
                    Intent intent = new Intent();
                    intent.setAction(android.provider.Settings.ACTION_BLUETOOTH_SETTINGS);
                    mContext.startActivity(intent);
                    break;
                }
                case ADAPTER_OFF_HELP: {
                    nativeShowBluetoothAdapterOffLink(mNativeBluetoothChooserDialogPtr);
                    closeDialog();
                    break;
                }
                case REQUEST_LOCATION_PERMISSION: {
                    mWindowAndroid.requestPermissions(
                            new String[] {Manifest.permission.ACCESS_COARSE_LOCATION},
                            BluetoothChooserDialog.this);
                    break;
                }
                case NEED_LOCATION_PERMISSION_HELP: {
                    nativeShowNeedLocationPermissionLink(mNativeBluetoothChooserDialogPtr);
                    closeDialog();
                    break;
                }
                case RESTART_SEARCH: {
                    mItemChooserDialog.clear();
                    nativeRestartSearch(mNativeBluetoothChooserDialogPtr);
                    break;
                }
                default:
                    assert false;
            }

            // Get rid of the highlight background on selection.
            view.invalidate();
        }

        @Override
        public void updateDrawState(TextPaint textPaint) {
            super.updateDrawState(textPaint);
            textPaint.bgColor = Color.TRANSPARENT;
            textPaint.setUnderlineText(false);
        }
    }

    @CalledByNative
    private static BluetoothChooserDialog create(WindowAndroid windowAndroid, String origin,
            int securityLevel, long nativeBluetoothChooserDialogPtr) {
        if (!windowAndroid.hasPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                && !windowAndroid.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                && !windowAndroid.canRequestPermission(
                           Manifest.permission.ACCESS_COARSE_LOCATION)) {
            // If we can't even ask for enough permission to scan for Bluetooth devices, don't open
            // the dialog.
            return null;
        }
        BluetoothChooserDialog dialog = new BluetoothChooserDialog(
                windowAndroid, origin, securityLevel, nativeBluetoothChooserDialogPtr);
        dialog.show();
        return dialog;
    }

    @CalledByNative
    private void addDevice(String deviceId, String deviceName) {
        List<ItemChooserDialog.ItemChooserRow> devices =
                new ArrayList<ItemChooserDialog.ItemChooserRow>();
        devices.add(new ItemChooserDialog.ItemChooserRow(deviceId, deviceName));
        mItemChooserDialog.addItemsToList(devices);
    }

    @CalledByNative
    private void closeDialog() {
        mNativeBluetoothChooserDialogPtr = 0;
        mItemChooserDialog.dismiss();
    }

    @CalledByNative
    private void removeDevice(String deviceId) {
        mItemChooserDialog.setEnabled(deviceId, false);
    }

    @CalledByNative
    private void notifyAdapterTurnedOff() {
        SpannableString adapterOffMessage = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_adapter_off),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(LinkType.ADAPTER_OFF, mContext)));
        SpannableString adapterOffStatus = SpanApplier.applySpans(
                mContext.getString(R.string.bluetooth_adapter_off_help),
                new SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(LinkType.ADAPTER_OFF_HELP, mContext)));

        mItemChooserDialog.setErrorState(adapterOffMessage, adapterOffStatus);
    }

    @CalledByNative
    private void notifyAdapterTurnedOn() {
        mItemChooserDialog.clear();
        if (mNativeBluetoothChooserDialogPtr != 0) {
            nativeRestartSearch(mNativeBluetoothChooserDialogPtr);
        }
    }

    @CalledByNative
    private void notifyDiscoveryState(int discoveryState) {
        switch (discoveryState) {
            case DISCOVERY_FAILED_TO_START: {
                // FAILED_TO_START might be caused by a missing Location permission.
                // Check, and show a request if so.
                checkLocationPermission();
                break;
            }
            case DISCOVERY_IDLE: {
                mItemChooserDialog.setIdleState();
                break;
            }
            default: {
                // TODO(jyasskin): Report the new state to the user.
                break;
            }
        }
    }

    private native void nativeOnDialogFinished(
            long nativeBluetoothChooserAndroid, int eventType, String deviceId);
    private native void nativeRestartSearch(long nativeBluetoothChooserAndroid);
    // Help links.
    private native void nativeShowBluetoothOverviewLink(long nativeBluetoothChooserAndroid);
    private native void nativeShowBluetoothAdapterOffLink(long nativeBluetoothChooserAndroid);
    private native void nativeShowNeedLocationPermissionLink(long nativeBluetoothChooserAndroid);
}
