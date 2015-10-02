// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.content.Intent;
import android.os.Bundle;
import android.os.Parcel;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.ExecutionException;

/**
 * Test various Java WebContents specific features.
 * TODO(dtrainor): Add more testing for the WebContents methods.
 */
public class WebContentsTest extends ContentShellTestBase {
    private static final String TEST_URL_1 = "about://blank";
    private static final String WEB_CONTENTS_KEY = "WEBCONTENTSKEY";
    private static final String PARCEL_STRING_TEST_DATA = "abcdefghijklmnopqrstuvwxyz";

    /**
     * Check that {@link WebContents#isDestroyed()} works as expected.
     * TODO(dtrainor): Test this using {@link WebContents#destroy()} instead once it is possible to
     * build a {@link WebContents} directly in the content/ layer.
     *
     * @throws InterruptedException
     * @throws ExecutionException
     */
    /*
    @SmallTest
    https://crbug.com/538625
    */
    @DisabledTest
    public void testWebContentsIsDestroyedMethod() throws InterruptedException, ExecutionException {
        final ContentShellActivity activity = launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = activity.getActiveWebContents();

        assertFalse("WebContents incorrectly marked as destroyed", webContents.isDestroyed());

        // Launch a new shell.
        Shell originalShell = activity.getActiveShell();
        loadNewShell(TEST_URL_1);
        assertNotSame("New shell not created", activity.getActiveShell(), originalShell);

        assertTrue("WebContents incorrectly marked as not destroyed", webContents.isDestroyed());
    }

    /**
     * Check that it is possible to serialize and deserialize a WebContents object through Parcels.
     *
     * @throws InterruptedException
     */
    @SmallTest
    public void testWebContentsSerializeDeserializeInParcel() throws InterruptedException {
        launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = getWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure they're equal.
            assertEquals("Deserialized object does not match",
                    webContents, deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that it is possible to serialize and deserialize a WebContents object through Bundles.
     * @throws InterruptedException
     */
    public void testWebContentsSerializeDeserializeInBundle() throws InterruptedException {
        launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = getWebContents();

        // Use a parcel to force the Bundle to actually serialize and deserialize, otherwise it can
        // cache the WebContents object.
        Parcel parcel = Parcel.obtain();

        try {
            // Create a bundle and put the WebContents in it.
            Bundle bundle = new Bundle();
            bundle.putParcelable(WEB_CONTENTS_KEY, webContents);

            // Serialize the Bundle.
            parcel.writeBundle(bundle);

            // Read back the Bundle.
            parcel.setDataPosition(0);
            Bundle deserializedBundle = parcel.readBundle();

            // Read back the WebContents.
            deserializedBundle.setClassLoader(WebContents.class.getClassLoader());
            WebContents deserializedWebContents =
                    deserializedBundle.getParcelable(WEB_CONTENTS_KEY);

            // Make sure they're equal.
            assertEquals("Deserialized object does not match",
                    webContents, deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that it is possible to serialize and deserialize a WebContents object through Intents.
     * @throws InterruptedException
     */
    public void testWebContentsSerializeDeserializeInIntent() throws InterruptedException {
        launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = getWebContents();

        // Use a parcel to force the Intent to actually serialize and deserialize, otherwise it can
        // cache the WebContents object.
        Parcel parcel = Parcel.obtain();

        try {
            // Create an Intent and put the WebContents in it.
            Intent intent = new Intent();
            intent.putExtra(WEB_CONTENTS_KEY, webContents);

            // Serialize the Intent
            parcel.writeParcelable(intent, 0);

            // Read back the Intent.
            parcel.setDataPosition(0);
            Intent deserializedIntent = parcel.readParcelable(null);

            // Read back the WebContents.
            deserializedIntent.setExtrasClassLoader(WebContents.class.getClassLoader());
            WebContents deserializedWebContents =
                    (WebContents) deserializedIntent.getParcelableExtra(WEB_CONTENTS_KEY);

            // Make sure they're equal.
            assertEquals("Deserialized object does not match",
                    webContents, deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that attempting to deserialize a WebContents object from a Parcel from another process
     * instance fails.
     * @throws InterruptedException
     */
    @SmallTest
    public void testWebContentsFailDeserializationAcrossProcessBoundary()
            throws InterruptedException {
        launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = getWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Invalidate all serialized WebContents.
            WebContentsImpl.invalidateSerializedWebContentsForTesting();

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            assertNull("Unexpectedly deserialized a WebContents", deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that serializing a destroyed WebContents always results in a null deserialized
     * WebContents.
     * @throws InterruptedException
     * @throws ExecutionException
     */
    /*
    @SmallTest
    https://crbug.com/538625
    */
    @DisabledTest
    public void testSerializingADestroyedWebContentsDoesNotDeserialize()
            throws InterruptedException, ExecutionException {
        ContentShellActivity activity = launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = activity.getActiveWebContents();
        loadNewShell(TEST_URL_1);

        assertTrue("WebContents not destroyed", webContents.isDestroyed());

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            assertNull("Unexpectedly deserialized a destroyed WebContents",
                    deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that destroying a WebContents after serializing it always results in a null
     * deserialized WebContents.
     * @throws InterruptedException
     * @throws ExecutionException
     */
    /*
    @SmallTest
    https://crbug.com/538625
    */
    @DisabledTest
    public void testDestroyingAWebContentsAfterSerializingDoesNotDeserialize()
            throws InterruptedException, ExecutionException {
        ContentShellActivity activity = launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = activity.getActiveWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Destroy the WebContents.
            loadNewShell(TEST_URL_1);
            assertTrue("WebContents not destroyed", webContents.isDestroyed());

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            assertNull("Unexpectedly deserialized a destroyed WebContents",
                    deserializedWebContents);
        } finally {
            parcel.recycle();
        }
    }

    /**
     * Check that failing a WebContents deserialization doesn't corrupt subsequent data in the
     * Parcel.
     * @throws InterruptedException
     */
    @SmallTest
    public void testFailedDeserializationDoesntCorruptParcel()
            throws InterruptedException {
        launchContentShellWithUrl(TEST_URL_1);
        waitForActiveShellToBeDoneLoading();
        WebContents webContents = getWebContents();

        Parcel parcel = Parcel.obtain();

        try {
            // Serialize the WebContents.
            parcel.writeParcelable(webContents, 0);

            // Serialize a String after the WebContents.
            parcel.writeString(PARCEL_STRING_TEST_DATA);

            // Invalidate all serialized WebContents.
            WebContentsImpl.invalidateSerializedWebContentsForTesting();

            // Try to read back the WebContents.
            parcel.setDataPosition(0);
            WebContents deserializedWebContents = parcel.readParcelable(
                    WebContents.class.getClassLoader());

            // Make sure we weren't able to deserialize the WebContents.
            assertNull("Unexpectedly deserialized a WebContents", deserializedWebContents);

            // Make sure we can properly deserialize the String after the WebContents.
            assertEquals("Failing to read the WebContents corrupted the parcel",
                    PARCEL_STRING_TEST_DATA, parcel.readString());
        } finally {
            parcel.recycle();
        }
    }
}
