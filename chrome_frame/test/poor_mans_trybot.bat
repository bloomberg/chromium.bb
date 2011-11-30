@ECHO OFF
REM Copyright (c) 2009 The Chromium Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
REM
REM Purpose of this file: If you have IE9 on your machine, not all of
REM the Chrome Frame tests will currently pass. To run the tests on
REM a VM or separate machine, you need to copy a bunch of things over
REM (the tests run slowly or flakily over a network drive).
REM
REM To facilitate running the tests on a separate machine, Run this
REM batch file on a VM or a separate machine, that has a drive mapped
REM to your dev machine (so that it can access your dev workspace).
REM
REM Before running this file, you need to build all the chrome_frame
REM targets plus chrome.dll and chrome.exe on your dev machine. Run
REM the copy of this file that exists in the same workspace that you
REM want to test.
REM
REM NOTE: I've seen cases where a Vista VM under Virtual PC will fail
REM to copy all of the files from the dev machine, and doing e.g.
REM [ dir j:\src\chrome\src\chrome\debug ] (assuming j:\src\chrome\src
REM is your workspace on the dev machine) shows an empty filing listing
REM when run from the VM, whereas your workspace actually just built
REM a ton of stuff in that directory. Just be aware of it, and check
REM what files you actually have in the local copy on the VM after
REM running this script. If you encounter this issue, a reboot of the
REM VM seems to help.
REM
REM ALSO NOTE: At least one test
REM (ChromeFrameTestWithWebServer.FullTabModeIE_TestPostReissue) fails
REM until you choose "never show again" in the "did you notice the
REM infobar" dialog that IE shows, at least if you're running IE7.

if (%1)==() goto usage

setlocal
set CLIENT_ROOT=%~dp0%..\..
set CONFIG=%1

if (%2)==() goto setdefault
if (%3)==() goto usage
set DRIVE=%2
set INSTALL_ROOT=%3
goto pastbase
:setdefault
set DRIVE=c:
set INSTALL_ROOT=\trybot
:pastbase

@ECHO ON
%DRIVE%
mkdir %INSTALL_ROOT%
cd %INSTALL_ROOT%
rmdir /s /q base
rmdir /s /q build\%CONFIG%
rmdir /s /q chrome_frame
mkdir base
mkdir build\%CONFIG%
mkdir chrome_frame\test\data
mkdir chrome_frame\test\html_util_test_data
copy %CLIENT_ROOT%\base\base_paths_win.cc base\base_paths_win.cc
xcopy %CLIENT_ROOT%\build\%CONFIG% build\%CONFIG% /E /EXCLUDE:%CLIENT_ROOT%\chrome_frame\test\poor_mans_trybot_xcopy_filter.txt
xcopy %CLIENT_ROOT%\chrome_frame\test\data chrome_frame\test\data /E
xcopy %CLIENT_ROOT%\chrome_frame\test\html_util_test_data chrome_frame\test\html_util_test_data /E
copy %CLIENT_ROOT%\chrome_frame\CFInstance.js chrome_frame\CFInstance.js
copy %CLIENT_ROOT%\chrome_frame\CFInstall.js chrome_frame\CFInstall.js
@ECHO OFF
echo ************************************
echo DO THE FOLLOWING IN AN ADMIN PROMPT:
echo ************************************
echo regsvr32 \trybot\build\%CONFIG%\servers\npchrome_frame.dll
echo *********************************
echo THEN DO THIS IN A REGULAR PROMPT:
echo *********************************
echo \trybot\build\%CONFIG%\chrome_frame_unittests.exe
echo \trybot\build\%CONFIG%\chrome_frame_tests.exe
goto end

:usage
echo "Usage: poor_mans_trybot.bat CONFIG [DRIVE INSTALL_ROOT]"

:end
