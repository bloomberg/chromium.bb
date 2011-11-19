@echo off

:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal

set NACL_SDK_ROOT=%~dp0

:: Preserve a copy of the PATH (in case we need it later, mainly for cygwin).
set PRESCONS_PATH=%PATH%

:: Set the PYTHONPATH and SCONS_LIB_DIR so we can import SCons modules
set SCONS_LIB_DIR=%~dp0third_party\scons-2.0.1\engine
set PYTHONPATH=%~dp0third_party\scons-2.0.1\engine;%~dp0third_party\native_client\native_client\build

:: We have to do this because scons overrides PYTHONPATH and does not preserve
:: what is provided by the OS.  The custom variable name won't be overwritten.
set PYMOX=%~dp0third_party\pymox

:: Stop incessant CYGWIN complains about "MS-DOS style path"
set CYGWIN=nodosfilewarning %CYGWIN%

:: Run the included copy of scons.
python -O -OO "%~dp0third_party\scons-2.0.1\script\scons" --file=main.scons %*

:end
