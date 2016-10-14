// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.os.Build;
import android.view.View;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.Callable;

/**
 * A collection of methods to aid in creating Render Tests - tests that render UI components.
 */
public class RenderUtils {
    private static final String TAG = "RenderUtils";

    private static final String FAILURE_FOLDER_RELATIVE = "/failures";

    /**
     * This is a list of devices that we maintain golden images for. If render tests are being run
     * on a device in this list, golden images should exist and their absence is a test failure.
     * The absence of golden images for devices not on this list doesn't necessarily mean that
     * something is wrong - the tests are just being run on a device that we don't have goldens for.
     */
    private static final String[] RENDER_TEST_DEVICES = { "Nexus 5X", "Nexus 9" };

    /**
     * Before we know how flaky screenshot tests are going to be we don't want them to cause a
     * full test failure every time they fail. If the tests prove their worth, this will be set to
     * false/removed.
     */
    private static final boolean REPORT_ONLY_DO_NOT_FAIL = true;

    private enum ComparisonResult { MATCH, MISMATCH, GOLDEN_NOT_FOUND }

    /**
     * An exception thrown when a View is checked and it is not equal to the golden.
     */
    public static class ImageMismatchException extends RuntimeException {
        public ImageMismatchException(String message) {
            super(message);
        }
    }

    /**
     * An exception thrown when a View is checked and the golden could not be found.
     */
    public static class GoldenNotFoundException extends RuntimeException {
        public GoldenNotFoundException(String message) {
            super(message);
        }
    }

    /**
     * A class to contain all the TestClass level information associated with rendering views.
     */
    public static class ViewRenderer {
        private final Activity mActivity;
        private final String mGoldenFolder;
        private final String mTestClass;

        public ViewRenderer(Activity activity, String goldenFolder, String testClass) {
            mActivity = activity;
            mGoldenFolder = goldenFolder;
            mTestClass = testClass;
        }

        /**
         * Renders the |view| and compares it to a golden view with the |id|. If the rendered image
         * does not match the golden, {@link ImageMismatchException} is thrown. If the golden image
         * is not found {@link GoldenNotFoundException} will be thrown if the test is running on a
         * device where goldens are expected to be present.
         *
         * The 'regenerate-goldens' flag provided to test_runner.py changes the behaviour of this
         * method. In this case the if the image does not match the golden or the golden is not
         * found the rendered image is saved to the device and nothing is thrown.
         *
         * @throws IOException if the rendered image could not be saved to the device.
         */
        public void renderAndCompare(final View view, String id) throws IOException {
            // Compare the image against the Golden.
            Bitmap bitmap = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Bitmap>() {
                @Override
                public Bitmap call() throws Exception {
                    return UiUtils.generateScaledScreenshot(view, 0, Bitmap.Config.ARGB_8888);
                }
            });

            String imagename = imageName(mActivity, mTestClass, id);
            File goldenFile = createPath(mGoldenFolder, imagename);
            ComparisonResult result = compareBmpToGolden(bitmap, goldenFile);

            if (REPORT_ONLY_DO_NOT_FAIL && !isGenerateMode()) {
                Log.d(TAG, "Image comparison for %s: %s", id, result.name());
                return;
            }

            if (result == ComparisonResult.MATCH) {
                if (isGenerateMode()) {
                    Log.i(TAG, "%s - generated image equal to golden.", id);
                }
                return;
            }

            // Save the rendered View.
            File failureFile = createPath(mGoldenFolder + FAILURE_FOLDER_RELATIVE, imagename);
            saveBitmap(bitmap, failureFile);

            if (isGenerateMode()) {
                Log.i(TAG, "%s - generated image saved to %s.", id, failureFile.getAbsolutePath());
                return;
            }

            if (result == ComparisonResult.GOLDEN_NOT_FOUND) {
                if (onRenderTestDevice()) {
                    throw new GoldenNotFoundException(
                            String.format("Could not find golden image for %s.",
                                    goldenFile.getAbsolutePath()));
                } else {
                    Log.i(TAG, "Could not find golden for %s, but not on a Render Test Device so "
                            + "continue.", goldenFile.getAbsolutePath());
                }
            } else if (result == ComparisonResult.MISMATCH) {
                throw new ImageMismatchException(
                        String.format("Image comparison failed on %s. Failure image saved to %s.",
                                id, failureFile.getAbsolutePath()));

            }
        }
    }

    /**
     * Whether the tests are being run to generate golden files or to check for equality.
     */
    private static boolean isGenerateMode() {
        return CommandLine.getInstance().hasSwitch("regenerate-goldens");
    }

    /**
     * Returns whether goldens should exist for the current device.
     */
    private static boolean onRenderTestDevice() {
        for (String model : RENDER_TEST_DEVICES) {
            if (model.equals(Build.MODEL)) return true;
        }
        return false;
    }

    /**
     * Creates an image name combining the image description with details about the device
     * (eg model, current orientation).
     */
    private static String imageName(Activity activity, String testClass, String desc) {
        Point outSize = new Point();
        activity.getWindowManager().getDefaultDisplay().getSize(outSize);
        String orientation = outSize.x > outSize.y ? "land" : "port";
        return String.format("%s.%s.%s.%s.png",
                testClass, desc, Build.MODEL.replace(' ', '_'), orientation);
    }

    /**
     * Saves a the given |bitmap| to the |file|.
     */
    private static void saveBitmap(Bitmap bitmap, File file) throws IOException {
        FileOutputStream out = new FileOutputStream(file);
        try {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        } finally {
            out.close();
        }
    }

    /**
     * Convenience method to create a File pointing to |filename| in |folder| in the external
     * storage directory.
     * @throws IOException
     */
    private static File createPath(String folder, String filename) throws IOException {
        File path = new File(UrlUtils.getIsolatedTestFilePath(folder));
        if (!path.exists()) {
            if (!path.mkdir()) {
                throw new IOException("Could not create " + path.getAbsolutePath());
            }
        }
        return new File(path + "/" + filename);
    }

    /**
     * Returns whether the given |bitmap| is equal to the one stored in |file|.
     */
    private static ComparisonResult compareBmpToGolden(Bitmap bitmap, File file) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = bitmap.getConfig();
        Bitmap golden = BitmapFactory.decodeFile(file.getAbsolutePath(), options);

        if (golden == null) return ComparisonResult.GOLDEN_NOT_FOUND;
        return bitmap.sameAs(golden) ? ComparisonResult.MATCH : ComparisonResult.MISMATCH;
    }
}
