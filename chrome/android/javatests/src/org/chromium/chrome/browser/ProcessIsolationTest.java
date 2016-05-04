// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test to make sure browser and renderer are seperated process.
 */
public class ProcessIsolationTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private Pattern mUidPattern;

    public ProcessIsolationTest() {
        super(ChromeActivity.class);
    }

    /**
     * Verifies that process isolation works, i.e., that the browser and
     * renderer processes use different user IDs.
     * @throws InterruptedException
     */
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = 22, message = "crbug.com/517611")
    @Feature({"Browser", "Security"})
    public void testProcessIsolationForRenderers() throws InterruptedException {
        int tabsCount = getActivity().getCurrentTabModel().getCount();
        // The ActivityManager can be used to retrieve the current processes, but the reported UID
        // in the RunningAppProcessInfo for isolated processes is the same as the parent process
        // (see b/7724486, closed as "Working as intended").
        // So we have to resort to parsing the ps output.
        String packageName = BuildInfo.getPackageName(getInstrumentation().getTargetContext());
        assertFalse("Failed to retrieve package name for current version of Chrome.",
                    TextUtils.isEmpty(packageName));

        ArrayList<String> uids = new ArrayList<String>();
        BufferedReader reader = null;
        boolean hasBrowserProcess = false;
        int rendererProcessesCount = 0;
        StringBuilder sb = new StringBuilder();
        try {
            Process psProcess = Runtime.getRuntime().exec("ps");
            reader = new BufferedReader(new InputStreamReader(psProcess.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
                if (line.indexOf(packageName) != -1) {
                    String uid = retrieveUid(line);
                    assertNotNull("Failed to retrieve UID from " + line, uid);
                    if (line.indexOf("sandboxed_process") != -1) {
                        // Renderer process.
                        uids.add(uid);
                        rendererProcessesCount++;
                    } else if (line.indexOf(packageName + ".") == -1) {
                        // Browser process.
                        if (!hasBrowserProcess) {
                            // On certain versions of Android 'ps' itself can appear in the list of
                            // processes with the same name as its parent (which is the browser
                            // process). Only the first occurence of the browser process is kept.
                            // Note that it is guaranteed that the first occurence corresponds to
                            // the browser process rather than 'ps' (its child) since processes in
                            // ps' output are sorted by ascending PID.
                            uids.add(uid);
                            hasBrowserProcess = true;
                        }
                    }
                }
            }
        } catch (IOException ioe) {
            fail("Failed to read ps output.");
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException ioe) {
                    // Intentionally do nothing.
                }
            }
        }
        assertTrue("Browser process not found in ps output: \n" + sb.toString(),
                 hasBrowserProcess);

        // We should have the same number of process as tabs count. Sometimes
        // there can be extra utility sandbox process so we check for greater than.
        assertTrue(
                "Renderer processes not found in ps output: \n" + sb.toString(),
                rendererProcessesCount >= tabsCount);

        assertEquals("Found at least two processes with the same UID in ps output: \n"
                + sb.toString(),
                uids.size(), new HashSet<String>(uids).size());
    }

    private String retrieveUid(String psLine) {
        if (mUidPattern == null) {
            mUidPattern = Pattern.compile("^\\S+");
        }
        Matcher m = mUidPattern.matcher(psLine);
        if (!m.find()) return null;
        return m.group(0);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }
}
