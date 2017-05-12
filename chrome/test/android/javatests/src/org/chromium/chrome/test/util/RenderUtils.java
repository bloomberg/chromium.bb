// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
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

    private static final String DIFF_FOLDER_RELATIVE = "/diffs";

    private static final String FAILURE_FOLDER_RELATIVE = "/failures";

    private static final String GOLDEN_FOLDER_RELATIVE = "/goldens";

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

    /**
     * How many pixels can be different in an image before counting the images as different.
     */
    private static final int PIXEL_DIFF_THRESHOLD = 0;

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
        private String mOutputDirectory;

        public ViewRenderer(Activity activity, String goldenFolder, String testClass) {
            mActivity = activity;
            mGoldenFolder = goldenFolder;
            mTestClass = testClass;

            // Render test will output results to subdirectory of |goldenFolder| unless
            // --render-test-output-dir is passed.
            mOutputDirectory = CommandLine.getInstance().getSwitchValue("render-test-output-dir");
            if (mOutputDirectory == null) {
                mOutputDirectory = UrlUtils.getIsolatedTestFilePath(mGoldenFolder);
            }
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
            Bitmap testBitmap =
                    ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Bitmap>() {
                        @Override
                        public Bitmap call() throws Exception {
                            return UiUtils.generateScaledScreenshot(
                                    view, 0, Bitmap.Config.ARGB_8888);
                        }
                    });

            String imagename = imageName(mActivity, mTestClass, id);

            File goldenFile =
                    createPath(UrlUtils.getIsolatedTestFilePath(mGoldenFolder), imagename);
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = testBitmap.getConfig();
            Bitmap goldenBitmap = BitmapFactory.decodeFile(goldenFile.getAbsolutePath(), options);

            Bitmap diffBitmap = null;
            ComparisonResult result = null;

            if (goldenBitmap == null) {
                result = ComparisonResult.GOLDEN_NOT_FOUND;
            } else {
                diffBitmap = Bitmap.createBitmap(
                        Math.max(testBitmap.getWidth(), goldenBitmap.getWidth()),
                        Math.max(testBitmap.getHeight(), goldenBitmap.getHeight()),
                        testBitmap.getConfig());
                result = compareBitmapToGolden(testBitmap, goldenBitmap, diffBitmap);
            }

            if (result == ComparisonResult.MATCH) {
                if (isGenerateMode()) {
                    Log.i(TAG, "%s - generated image equal to golden.", id);
                }
                return;
            }

            File failureOutputFile =
                    createPath(mOutputDirectory + FAILURE_FOLDER_RELATIVE, imagename);
            saveBitmap(testBitmap, failureOutputFile);

            if (result != ComparisonResult.GOLDEN_NOT_FOUND) {
                File goldenOutputFile =
                        createPath(mOutputDirectory + GOLDEN_FOLDER_RELATIVE, imagename);
                saveBitmap(goldenBitmap, goldenOutputFile);

                File diffOutputFile =
                        createPath(mOutputDirectory + DIFF_FOLDER_RELATIVE, imagename);
                saveBitmap(diffBitmap, diffOutputFile);
            }

            if (isGenerateMode()) {
                Log.i(TAG, "%s - generated image saved to %s.", id,
                        failureOutputFile.getAbsolutePath());
                return;
            }

            if (REPORT_ONLY_DO_NOT_FAIL) {
                Log.d(TAG, "Image comparison for %s: %s", id, result.name());
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
                                id, failureOutputFile.getAbsolutePath()));
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
     *
     * This function must be kept in sync with |RE_RENDER_IMAGE_NAME| from
     * src/build/android/pylib/local/device/local_device_instrumentation_test_run.py.
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
     * Convenience method to create a File pointing to |filename| in |folder|.
     * @throws IOException
     */
    private static File createPath(String folder, String filename) throws IOException {
        File path = new File(folder);
        if (!path.exists()) {
            if (!path.mkdirs()) {
                throw new IOException("Could not create " + path.getAbsolutePath());
            }
        }
        return new File(path + "/" + filename);
    }

    /**
     * Returns whether the given |bitmap| is equal to the one stored in |file|.
     */
    private static ComparisonResult compareBitmapToGolden(Bitmap test, Bitmap golden, Bitmap diff) {
        int maxWidth = Math.max(test.getWidth(), golden.getWidth());
        int maxHeight = Math.max(test.getHeight(), golden.getHeight());
        int minWidth = Math.min(test.getWidth(), golden.getWidth());
        int minHeight = Math.min(test.getHeight(), golden.getHeight());

        int diffPixelsCount = comparePixels(test, golden, diff, 0, 0, minWidth, 0, minHeight)
                + compareSizes(diff, minWidth, maxWidth, minHeight, maxHeight);

        if (diffPixelsCount > PIXEL_DIFF_THRESHOLD) {
            return ComparisonResult.MISMATCH;
        }
        return ComparisonResult.MATCH;
    }

    /**
     * Compares two bitmaps pixel-wise.
     *
     * @param testImage Bitmap of test image.
     *
     * @param goldenImage Bitmap of golden image.
     *
     * @param diffImage This is an output argument. Function will set pixels in the |diffImage| to
     * either transparent or red depending on whether that pixel differed in the golden and test
     * bitmaps. diffImage should have its width and height be the max width and height of the
     * golden and test bitmaps.
     *
     * @param diffThreshold Threshold for when to consider two color values as different. These
     * values are 8 bit (256) so this threshold value should be in range 0-256.
     *
     * @param startWidth Start x-coord to start diffing the Bitmaps.
     *
     * @param endWidth End x-coord to start diffing the Bitmaps.
     *
     * @param startHeight Start y-coord to start diffing the Bitmaps.
     *
     * @param endHeight End x-coord to start diffing the Bitmaps.
     *
     * @return Returns number of pixels that differ between |goldenImage| and |testImage|
     */
    private static int comparePixels(Bitmap testImage, Bitmap goldenImage, Bitmap diffImage,
            int diffThreshold, int startWidth, int endWidth, int startHeight, int endHeight) {
        int diffPixels = 0;

        for (int x = startWidth; x < endWidth; x++) {
            for (int y = startHeight; y < endHeight; y++) {
                int goldenImageColor = goldenImage.getPixel(x, y);
                int testImageColor = testImage.getPixel(x, y);

                int redDiff = Math.abs(Color.red(goldenImageColor) - Color.red(testImageColor));
                int blueDiff =
                        Math.abs(Color.green(goldenImageColor) - Color.green(testImageColor));
                int greenDiff = Math.abs(Color.blue(goldenImageColor) - Color.blue(testImageColor));
                int alphaDiff =
                        Math.abs(Color.alpha(goldenImageColor) - Color.alpha(testImageColor));

                if (redDiff > diffThreshold || blueDiff > diffThreshold || greenDiff > diffThreshold
                        || alphaDiff > diffThreshold) {
                    diffPixels++;
                    diffImage.setPixel(x, y, Color.RED);
                } else {
                    diffImage.setPixel(x, y, Color.TRANSPARENT);
                }
            }
        }
        return diffPixels;
    }

    /**
     * Compares two bitmaps size.
     *
     * @param diffImage This is an output argument. Function will set pixels in the |diffImage| to
     * either transparent or red depending on whether that pixel coordinate occurs in the
     * dimensions of the golden and not the test bitmap or vice-versa.
     *
     * @param minWidth Min width of golden and test bitmaps.
     *
     * @param maxWidth Max width of golden and test bitmaps.
     *
     * @param minHeight Min height of golden and test bitmaps.
     *
     * @param maxHeight Max height of golden and test bitmaps.
     *
     * @return Returns number of pixels that differ between |goldenImage| and |testImage| due to
     * their size.
     */
    private static int compareSizes(
            Bitmap diffImage, int minWidth, int maxWidth, int minHeight, int maxHeight) {
        int diffPixels = 0;
        if (maxWidth > minWidth) {
            for (int x = minWidth; x < maxWidth; x++) {
                for (int y = 0; y < maxHeight; y++) {
                    diffImage.setPixel(x, y, Color.RED);
                }
            }
            diffPixels += (maxWidth - minWidth) * maxHeight;
        }
        if (maxHeight > minHeight) {
            for (int x = 0; x < maxWidth; x++) {
                for (int y = minHeight; y < maxHeight; y++) {
                    diffImage.setPixel(x, y, Color.RED);
                }
            }
            diffPixels += (maxHeight - minHeight) * minWidth;
        }
        return diffPixels;
    }
}
