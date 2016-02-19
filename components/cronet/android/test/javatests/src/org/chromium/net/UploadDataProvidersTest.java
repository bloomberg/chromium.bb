// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.ParcelFileDescriptor;
import android.os.StrictMode;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.io.FileOutputStream;

/** Test the default provided implementations of {@link UploadDataProvider} */
public class UploadDataProvidersTest extends CronetTestBase {
    private static final String LOREM = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
            + "Proin elementum, libero laoreet fringilla faucibus, metus tortor vehicula ante, "
            + "lacinia lorem eros vel sapien.";
    private CronetTestFramework mTestFramework;
    private File mFile;
    private StrictMode.VmPolicy mOldVmPolicy;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mOldVmPolicy = StrictMode.getVmPolicy();
        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                                       .detectLeakedClosableObjects()
                                       .penaltyLog()
                                       .penaltyDeath()
                                       .build());
        mTestFramework = startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
        // Add url interceptors after native application context is initialized.
        MockUrlRequestJobFactory.setUp();
        mFile = new File(getContext().getCacheDir().getPath() + "/tmpfile");
        FileOutputStream fileOutputStream = new FileOutputStream(mFile);
        try {
            fileOutputStream.write(LOREM.getBytes("UTF-8"));
        } finally {
            fileOutputStream.close();
        }
    }

    @SuppressFBWarnings("DM_GC") // Used to trigger strictmode detecting leaked closeables
    @Override
    protected void tearDown() throws Exception {
        try {
            NativeTestServer.shutdownNativeTestServer();
            mTestFramework.mCronetEngine.shutdown();
            assertTrue(mFile.delete());
            // Run GC and finalizers a few times to pick up leaked closeables
            for (int i = 0; i < 10; i++) {
                System.gc();
                System.runFinalization();
            }
            System.gc();
            System.runFinalization();
            super.tearDown();
        } finally {
            StrictMode.setVmPolicy(mOldVmPolicy);
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testFileProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);
        UploadDataProvider dataProvider = UploadDataProviders.create(mFile);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(LOREM, callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testFileDescriptorProvider() throws Exception {
        ParcelFileDescriptor descriptor =
                ParcelFileDescriptor.open(mFile, ParcelFileDescriptor.MODE_READ_ONLY);
        assertTrue(descriptor.getFileDescriptor().valid());
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);
        UploadDataProvider dataProvider = UploadDataProviders.create(descriptor);
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(LOREM, callback.mResponseAsString);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBadFileDescriptorProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);
        ParcelFileDescriptor[] pipe = ParcelFileDescriptor.createPipe();
        try {
            UploadDataProvider dataProvider = UploadDataProviders.create(pipe[0]);
            builder.setUploadDataProvider(dataProvider, callback.getExecutor());
            builder.addHeader("Content-Type", "useless/string");
            builder.build().start();
            callback.blockForDone();

            assertTrue(callback.mError.getCause() instanceof IllegalArgumentException);
        } finally {
            pipe[1].close();
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testBufferProvider() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                new UrlRequest.Builder(NativeTestServer.getRedirectToEchoBody(), callback,
                        callback.getExecutor(), mTestFramework.mCronetEngine);
        UploadDataProvider dataProvider = UploadDataProviders.create(LOREM.getBytes("UTF-8"));
        builder.setUploadDataProvider(dataProvider, callback.getExecutor());
        builder.addHeader("Content-Type", "useless/string");
        builder.build().start();
        callback.blockForDone();

        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals(LOREM, callback.mResponseAsString);
    }
}
