REM Copyright (c) 2010 The Chromium Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

echo Attempting to register CEEE components
echo If something fails make sure that the files exist and that you're
echo running from an administrator command prompt.
pushd %~dp0%..\chrome\Debug\servers
regsvr32 /s npchrome_frame.dll
regsvr32 /s ceee_ie.dll
regsvr32 /s ceee_installer_helper.dll
popd
