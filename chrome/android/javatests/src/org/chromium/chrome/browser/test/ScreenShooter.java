// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import static org.hamcrest.Matchers.isIn;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.content.res.Configuration;
import android.graphics.Point;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeVersionInfo;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Rule for taking screen shots within tests. Screenshots are saved as
 * {@code screenshot_dir/test_class_directory/test_directory/shot_name random.png}.
 * The associated JSON file describing the screenshot is saved as
 * {@code screenshot_dir/test_class_directory/test_directory/shot_name random.json}.
 * <p>
 * {@code screenshot_dir} comes from the instrumentation test command line, which is set by the
 * test runners. {@code test_class_directory} and {@code test_directory} can both the set by the
 * {@link ScreenShooter.Directory} annotation. {@code test_class_directory} defaults to nothing
 * (i.e. no directory created at this level), and {@code test_directory} defaults to the name of
 * the individual test. {@code random} is a random value to make the filenames unique.
 * <p>
 * The JSON file contains three categories of data:
 * <dl>
 * <dt>filters</dt><dd><dd>System defined key/value pairs (e.g. the name of the test) that are
 *                          available for filtering test sets in the UiCatalogue</dd>
 * <dt>tags</dt><dd>User defined strings that further define the test. Tags include all features of
 *                  the test (as defined by the &#064Feature annotation), and all the tags provided
 *                  as arguments to the the {@code shoot} call</dd>
 * <dt>metadata</dt><dd>Other metadata (e.g. the exact time at which the test was run) that is not
 *                      suitable for filtering</dd>
 * </dl>
 * <p>
 * A simple example:
 * <p>
 * <pre>
 * &#064;RunWith(ChromeJUnit4ClassRunner.class)
 * &#064;CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
 * &#064;Restriction(RESTRICTION_TYPE_PHONE) // Tab switcher button only exists on phones.
 * &#064;ScreenShooter.Directory("Example")
 * public class ExampleUiCaptureTest {
 *     &#064;Rule
 *     public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
 *             new ChromeActivityTestRule<>(ChromeTabbedActivity.class);
 *
 *     &#064;Rule
 *     public ScreenShooter mScreenShooter = new ScreenShooter();
 *
 *     &#064;Before
 *     public void setUp() throws InterruptedException {
 *         mActivityTestRule.startMainActivityFromLauncher();
 *     }
 *
 *     // Capture the New Tab Page and the tab switcher.
 *     &#064;Test
 *     &#064;SmallTest
 *     &#064;Feature({"UiCatalogue"})
 *     &#064;ScreenShooter.Directory("TabSwitcher")
 *     public void testCaptureTabSwitcher() throws IOException, InterruptedException {
 *         mScreenShooter.shoot("NTP");
 *         Espresso.onView(ViewMatchers.withId(R.id.tab_switcher_button)).
 *                      perform(ViewActions.click());
 *         mScreenShooter.shoot("Tab switcher");
 *     }
 * }
 * </pre>
 */
public class ScreenShooter extends TestWatcher {
    private static final String SCREENSHOT_DIR =
            "org.chromium.base.test.util.Screenshooter.ScreenshotDir";
    private static final String IMAGE_SUFFIX = ".png";
    private static final String JSON_SUFFIX = ".json";

    // Filter keys
    private static final String TEST_CLASS_FILTER = "Test Class";
    private static final String TEST_METHOD_FILTER = "Test Method";
    private static final String SCREENSHOT_NAME_FILTER = "Screenshot Name";
    private static final String DEVICE_MODEL_FILTER = "Device Model";
    private static final String DISPLAY_SIZE_FILTER = "Display Size";
    private static final String ORIENTATION_FILTER = "Orientation";
    private static final String ANDROID_VERSION_FILTER = "Android Version";
    private static final String CHROME_VERSION_FILTER = "Chrome Version";
    private static final String CHROME_CHANNEL_FILTER = "Chrome Channel";
    private static final String LOCALE_FILTER = "Locale";
    // UPLOAD_TIME_FILTER is reserved for use by the Clank UI Catalogue uploader.
    private static final String UPLOAD_TIME_FILTER = "Upload Time";

    private final UiDevice mDevice;
    private final String mBaseDir;
    private File mDir;
    private String mTestClassName;
    private String mTestMethodName;
    private static final String[] FILTERS = {TEST_CLASS_FILTER, TEST_METHOD_FILTER,
            SCREENSHOT_NAME_FILTER, DEVICE_MODEL_FILTER, DISPLAY_SIZE_FILTER, ORIENTATION_FILTER,
            ANDROID_VERSION_FILTER, CHROME_VERSION_FILTER, CHROME_CHANNEL_FILTER, LOCALE_FILTER,
            UPLOAD_TIME_FILTER};
    private String[] mFeatures;

    /**
     * This Enum is a central list of all allowed tags. Please add to it if you need a new tag for
     * your screenshots.
     */
    public enum TagsEnum {
        UiCatalogueExample,
    }

    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.TYPE, ElementType.METHOD})
    public @interface Directory {
        String value();
    }

    public ScreenShooter() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mDevice = UiDevice.getInstance(instrumentation);
        mBaseDir = InstrumentationRegistry.getArguments().getString(SCREENSHOT_DIR);
    }

    @Override
    protected void starting(Description d) {
        mDir = new File(mBaseDir);
        mTestClassName = d.getClassName();
        mTestMethodName = d.getMethodName();
        Class<?> testClass = d.getTestClass();
        Directory classDirectoryAnnotation = testClass.getAnnotation(Directory.class);
        String classDirName =
                classDirectoryAnnotation == null ? "" : classDirectoryAnnotation.value();
        if (!classDirName.isEmpty()) mDir = new File(mBaseDir, classDirName);
        Directory methodDirectoryAnnotation = d.getAnnotation(Directory.class);
        String testMethodDir = methodDirectoryAnnotation == null
                ? d.getMethodName()
                : methodDirectoryAnnotation.value();
        if (!testMethodDir.isEmpty()) mDir = new File(mDir, testMethodDir);
        if (!mDir.exists()) assertTrue("Create screenshot directory", mDir.mkdirs());
        mFeatures = d.getAnnotation(Feature.class).value();
    }

    private static void setFilterValue(Map<String, String> tags, String name, String value) {
        assertThat("\"" + name + "\" is a known filter", name, isIn(FILTERS));
        tags.put(name, value);
    }

    /**
     * Take a screenshot and save it to a file, with tags and metadata in a JSON file
     *
     * @param shotName The name of this particular screenshot within this test.
     * @param tags User selected tags from {@link TagsEnum}.
     */
    public void shoot(String shotName, TagsEnum... tags) {
        assertNotNull("ScreenShooter rule initialized", mDir);
        Map<String, String> filters = new HashMap<>();
        setFilterValue(filters, TEST_CLASS_FILTER, mTestClassName);
        setFilterValue(filters, TEST_METHOD_FILTER, mTestMethodName);
        setFilterValue(filters, SCREENSHOT_NAME_FILTER, shotName);
        setFilterValue(filters, DEVICE_MODEL_FILTER, Build.MANUFACTURER + " " + Build.MODEL);
        Point displaySize = mDevice.getDisplaySizeDp();
        setFilterValue(filters, DISPLAY_SIZE_FILTER,
                String.format(Locale.US, "%d X %d", Math.min(displaySize.x, displaySize.y),
                        Math.max(displaySize.x, displaySize.y)));
        int orientation =
                InstrumentationRegistry.getContext().getResources().getConfiguration().orientation;
        setFilterValue(filters, ORIENTATION_FILTER,
                orientation == Configuration.ORIENTATION_LANDSCAPE ? "landscape" : "portrait");
        setFilterValue(filters, ANDROID_VERSION_FILTER, Build.VERSION.RELEASE);
        setFilterValue(filters, CHROME_VERSION_FILTER,
                Integer.toString(ChromeVersionInfo.getProductMajorVersion()));
        String channelName = "Unknown";
        if (ChromeVersionInfo.isLocalBuild()) {
            channelName = "Local Build";
        } else if (ChromeVersionInfo.isCanaryBuild()) {
            channelName = "Canary";
        } else if (ChromeVersionInfo.isBetaBuild()) {
            channelName = "Beta";
        } else if (ChromeVersionInfo.isDevBuild()) {
            channelName = "Dev";
        } else if (ChromeVersionInfo.isStableBuild()) {
            channelName = "Stable";
        }
        if (ChromeVersionInfo.isOfficialBuild()) {
            channelName = channelName + " Official";
        }
        setFilterValue(filters, CHROME_CHANNEL_FILTER, channelName);
        setFilterValue(filters, LOCALE_FILTER, Locale.getDefault().toString());

        Map<String, String> metadata = new HashMap<>();
        DateFormat formatter =
                DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT, Locale.US);
        metadata.put("Capture time (UTC)", formatter.format(new Date()));
        metadata.put("Chrome full product version", ChromeVersionInfo.getProductVersion());
        metadata.put("Android build fingerprint", Build.FINGERPRINT);

        try {
            File shotFile = File.createTempFile(shotName, IMAGE_SUFFIX, mDir);
            assertTrue("Screenshot " + shotName, mDevice.takeScreenshot(shotFile));
            writeImageDescription(shotFile, filters, tags, metadata);
        } catch (IOException e) {
            fail("Cannot create shot files " + e.toString());
        }
    }

    private void writeImageDescription(File shotFile, Map<String, String> filters, TagsEnum[] tags,
            Map<String, String> metadata) throws IOException {
        JSONObject imageDescription = new JSONObject();
        String shotFileName = shotFile.getName();
        List<String> tagStrings = new ArrayList<>();
        for (TagsEnum tag : tags) {
            tagStrings.add(tag.toString());
        }
        for (String feature : mFeatures) {
            tagStrings.add(feature + " Feature");
        }
        try {
            imageDescription.put("location", shotFileName);
            imageDescription.put("filters", new JSONObject(filters));
            imageDescription.put("tags", new JSONArray(tagStrings));
            imageDescription.put("metadata", new JSONObject(metadata));
        } catch (JSONException e) {
            fail("JSON error " + e.toString());
        }
        String jsonFileName =
                shotFileName.substring(0, shotFileName.length() - IMAGE_SUFFIX.length())
                + JSON_SUFFIX;
        try (FileWriter fileWriter = new FileWriter(new File(mDir, jsonFileName));) {
            fileWriter.write(imageDescription.toString());
        }
    }
}
