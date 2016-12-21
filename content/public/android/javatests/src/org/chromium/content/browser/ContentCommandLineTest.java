// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.MediumTest;

import org.chromium.base.CommandLine;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content_shell_apk.ContentShellApplication;

/**
 * Test class for command lines.
 */
public class ContentCommandLineTest extends NativeLibraryTestBase {
    // A reference command line. Note that switch2 is [brea\d], switch3 is [and "butter"],
    // and switch4 is [a "quoted" 'food'!]
    static final String INIT_SWITCHES[] = { "init_command", "--switch", "Arg",
        "--switch2=brea\\d", "--switch3=and \"butter\"",
        "--switch4=a \"quoted\" 'food'!",
        "--", "--actually_an_arg" };

    // The same command line, but in quoted string format.
    static final char INIT_SWITCHES_BUFFER[] =
        ("init_command --switch Arg --switch2=brea\\d --switch3=\"and \\\"butt\"er\\\"   "
        + "--switch4='a \"quoted\" \\'food\\'!' "
        + "-- --actually_an_arg").toCharArray();

    static final String CL_ADDED_SWITCH = "zappo-dappo-doggy-trainer";
    static final String CL_ADDED_SWITCH_2 = "username";
    static final String CL_ADDED_VALUE_2 = "bozo";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        CommandLine.reset();
    }

    void loadJni() {
        assertFalse(CommandLine.getInstance().isNativeImplementation());
        loadNativeLibraryNoBrowserProcess();
        assertTrue(CommandLine.getInstance().isNativeImplementation());
    }

    void checkInitSwitches() {
        CommandLine cl = CommandLine.getInstance();
        assertFalse(cl.hasSwitch("init_command"));
        assertTrue(cl.hasSwitch("switch"));
        assertFalse(cl.hasSwitch("--switch"));
        assertFalse(cl.hasSwitch("arg"));
        assertFalse(cl.hasSwitch("actually_an_arg"));
        assertEquals("brea\\d", cl.getSwitchValue("switch2"));
        assertEquals("and \"butter\"", cl.getSwitchValue("switch3"));
        assertEquals("a \"quoted\" 'food'!", cl.getSwitchValue("switch4"));
        assertNull(cl.getSwitchValue("switch"));
        assertNull(cl.getSwitchValue("non-existant"));
    }

    void checkSettingThenGetting() {
        CommandLine cl = CommandLine.getInstance();

        // Add a plain switch.
        assertFalse(cl.hasSwitch(CL_ADDED_SWITCH));
        cl.appendSwitch(CL_ADDED_SWITCH);
        assertTrue(cl.hasSwitch(CL_ADDED_SWITCH));

        // Add a switch paired with a value.
        assertFalse(cl.hasSwitch(CL_ADDED_SWITCH_2));
        assertNull(cl.getSwitchValue(CL_ADDED_SWITCH_2));
        cl.appendSwitchWithValue(CL_ADDED_SWITCH_2, CL_ADDED_VALUE_2);
        assertTrue(CL_ADDED_VALUE_2.equals(cl.getSwitchValue(CL_ADDED_SWITCH_2)));

        // Append a few new things.
        final String switchesAndArgs[] = { "dummy", "--superfast", "--speed=turbo" };
        assertFalse(cl.hasSwitch("dummy"));
        assertFalse(cl.hasSwitch("superfast"));
        assertNull(cl.getSwitchValue("speed"));
        cl.appendSwitchesAndArguments(switchesAndArgs);
        assertFalse(cl.hasSwitch("dummy"));
        assertFalse(cl.hasSwitch("command"));
        assertTrue(cl.hasSwitch("superfast"));
        assertTrue("turbo".equals(cl.getSwitchValue("speed")));
    }

    void checkAppendedSwitchesPassedThrough() {
        CommandLine cl = CommandLine.getInstance();
        assertTrue(cl.hasSwitch(CL_ADDED_SWITCH));
        assertTrue(cl.hasSwitch(CL_ADDED_SWITCH_2));
        assertTrue(CL_ADDED_VALUE_2.equals(cl.getSwitchValue(CL_ADDED_SWITCH_2)));
    }

    @MediumTest
    @Feature({"Android-AppBase"})
    public void testJavaNativeTransition() {
        CommandLine.init(INIT_SWITCHES);
        checkInitSwitches();
        loadJni();
        checkInitSwitches();
        checkSettingThenGetting();
    }

    @MediumTest
    @Feature({"Android-AppBase"})
    public void testJavaNativeTransitionAfterAppends() {
        CommandLine.init(INIT_SWITCHES);
        checkInitSwitches();
        checkSettingThenGetting();
        loadJni();
        checkInitSwitches();
        checkAppendedSwitchesPassedThrough();
    }

    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNativeInitialization() {
        CommandLine.init(null);
        loadJni();
        // Drop the program name for use with appendSwitchesAndArguments.
        String[] args = new String[INIT_SWITCHES.length - 1];
        System.arraycopy(INIT_SWITCHES, 1, args, 0, args.length);
        CommandLine.getInstance().appendSwitchesAndArguments(args);
        checkInitSwitches();
        checkSettingThenGetting();
    }

    @SuppressFBWarnings("DMI_HARDCODED_ABSOLUTE_FILENAME")
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testFileInitialization() {
        CommandLine.initFromFile(ContentShellApplication.COMMAND_LINE_FILE);
        loadJni();
        checkSettingThenGetting();
    }
}
