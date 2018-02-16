// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

/**
 * Manages tracing functionality in WebView.
 */
@JNINamespace("android_webview")
public class AwTracingController {
    private static final String TAG = "cr.AwTracingController";

    public static final int CATEGORIES_ALL = 0;
    public static final int CATEGORIES_ANDROID_WEBVIEW = 1;
    public static final int CATEGORIES_WEB_DEVELOPER = 2;
    public static final int CATEGORIES_INPUT_LATENCY = 3;
    public static final int CATEGORIES_RENDERING = 4;
    public static final int CATEGORIES_JAVASCRIPT_AND_RENDERING = 5;
    public static final int CATEGORIES_FRAME_VIEWER = 6;

    private static final List<String> CATEGORIES_ALL_LIST = new ArrayList<>(Arrays.asList("*"));
    private static final List<String> CATEGORIES_ANDROID_WEBVIEW_LIST =
            new ArrayList<>(Arrays.asList("blink", "android_webview", "Java", "toplevel"));
    private static final List<String> CATEGORIES_WEB_DEVELOPER_LIST = new ArrayList<>(
            Arrays.asList("blink", "cc", "netlog", "renderer.scheduler", "toplevel", "v8"));
    private static final List<String> CATEGORIES_INPUT_LATENCY_LIST = new ArrayList<>(
            Arrays.asList("benchmark", "input", "evdev", "renderer.scheduler", "toplevel"));
    private static final List<String> CATEGORIES_RENDERING_LIST =
            new ArrayList<>(Arrays.asList("blink", "cc", "gpu", "toplevel"));
    private static final List<String> CATEGORIES_JAVASCRIPT_AND_RENDERING_LIST = new ArrayList<>(
            Arrays.asList("blink", "cc", "gpu", "renderer.scheduler", "v8", "toplevel"));
    private static final List<String> CATEGORIES_FRAME_VIEWER_LIST = new ArrayList<>(
            Arrays.asList("blink", "cc", "gpu", "renderer.scheduler", "v8", "toplevel",
                    "disabled-by-default-cc.debug", "disabled-by-default-cc.debug.picture",
                    "disabled-by-default-cc.debug.display_items"));

    private static final List<List<String>> PREDEFINED_CATEGORIES_LIST =
            new ArrayList<List<String>>(Arrays.asList(CATEGORIES_ALL_LIST, // CATEGORIES_ALL
                    CATEGORIES_ANDROID_WEBVIEW_LIST, // CATEGORIES_ANDROID_WEBVIEW
                    CATEGORIES_WEB_DEVELOPER_LIST, // CATEGORIES_WEB_DEVELOPER
                    CATEGORIES_INPUT_LATENCY_LIST, // CATEGORIES_INPUT_LATENCY
                    CATEGORIES_RENDERING_LIST, // CATEGORIES_RENDERING
                    CATEGORIES_JAVASCRIPT_AND_RENDERING_LIST, // CATEGORIES_JAVASCRIPT_AND_RENDERING
                    CATEGORIES_FRAME_VIEWER_LIST // CATEGORIES_FRAME_VIEWER
                    ));

    // TODO(timvolodine): remove once glue layer is updated, crbug.com/812289.
    private AwTracingOutputStream mOutputStream;

    private OutputStream mOutputStreamNew;

    // TODO(timvolodine): consider caching a mIsTracing value for efficiency.
    // boolean mIsTracing;

    /**
     * Interface for the callbacks to return tracing data.
     */
    // TODO(timvolodine): remove once glue layer is updated, crbug.com/812289.
    public interface AwTracingOutputStream {
        public void write(byte[] chunk);
        public void complete();
    }

    public AwTracingController() {
        mNativeAwTracingController = nativeInit();
    }

    // Start tracing
    public void start(Collection<Integer> predefinedCategories,
            Collection<String> customIncludedCategories, int mode) {
        if (isTracing()) {
            throw new IllegalArgumentException("Cannot start tracing: tracing is already enabled");
        }
        String categoryFilter =
                constructCategoryFilterString(predefinedCategories, customIncludedCategories);
        nativeStart(mNativeAwTracingController, categoryFilter, mode);
    }

    // TODO(timvolodine): remove once glue layer is updated, crbug.com/812289.
    // Start tracing.
    public boolean start(String categoryFilter, int mode) {
        if (isTracing()) {
            return false;
        }

        boolean result = nativeStart(mNativeAwTracingController, categoryFilter, mode);
        return result;
    }

    // TODO(timvolodine): remove once glue layer is updated, crbug.com/812289.
    // Stop tracing and flush tracing data.
    public boolean stopAndFlush(@Nullable AwTracingOutputStream outputStream) {
        if (!isTracing()) return false;
        mOutputStream = outputStream;
        nativeStopAndFlush(mNativeAwTracingController);
        return true;
    }

    // Stop tracing and flush tracing data.
    public boolean stopAndFlush(@Nullable OutputStream outputStream) {
        if (!isTracing()) return false;
        mOutputStreamNew = outputStream;
        nativeStopAndFlush(mNativeAwTracingController);
        return true;
    }

    public boolean isTracing() {
        return nativeIsTracing(mNativeAwTracingController);
    }

    private String constructCategoryFilterString(
            Collection<Integer> predefinedCategories, Collection<String> customIncludedCategories) {
        // Make sure to remove any doubles in category patterns.
        HashSet<String> categoriesSet = new HashSet<String>();
        for (int predefinedCategoriesIndex : predefinedCategories) {
            categoriesSet.addAll(PREDEFINED_CATEGORIES_LIST.get(predefinedCategoriesIndex));
        }
        for (String categoryPattern : customIncludedCategories) {
            if (isValidPattern(categoryPattern)) {
                categoriesSet.add(categoryPattern);
            } else {
                throw new IllegalArgumentException(
                        "category patterns starting with '-' or containing ',' are not allowed");
            }
        }
        if (categoriesSet.isEmpty()) {
            // when no categories are specified -- exclude everything
            categoriesSet.add("-*");
        }
        return TextUtils.join(",", categoriesSet);
    }

    private boolean isValidPattern(String pattern) {
        // do not allow 'excluded' patterns or comma separated strings
        return !pattern.startsWith("-") && !pattern.contains(",");
    }

    @CalledByNative
    private void onTraceDataChunkReceived(byte[] data) throws IOException {
        // TODO(timvolodine): remove once glue layer is updated, crbug.com/812289.
        if (mOutputStream != null) {
            mOutputStream.write(data);
        }
        if (mOutputStreamNew != null) {
            mOutputStreamNew.write(data);
        }
    }

    @CalledByNative
    private void onTraceDataComplete() throws IOException {
        // TODO(timvolodine): remove once glue layer is updated, crbug.com/812289.
        if (mOutputStream != null) {
            mOutputStream.complete();
        }
        if (mOutputStreamNew != null) {
            mOutputStreamNew.close();
        }
    }

    private long mNativeAwTracingController;
    private native long nativeInit();
    private native boolean nativeStart(
            long nativeAwTracingController, String categories, int traceMode);
    private native boolean nativeStopAndFlush(long nativeAwTracingController);
    private native boolean nativeIsTracing(long nativeAwTracingController);
}
