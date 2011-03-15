echo on

call buildbot\msvs_env.bat

set MODE=%1
set BITS=%2

set RETCODE=0

if %MODE% equ "dbg" (set GYPMODE=Debug) else (set GYPMODE=Release)
if %BITS% equ 32 (set VCBITS=x86) else (set VCBITS=x64)

echo @@@BUILD_STEP gclient_runhooks@@@
cmd /c gclient runhooks
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP clobber@@@
rd /s /q scons-out & ^
 rd /s /q toolchain & ^
 rd /s /q build\Debug build\Release & ^
 rd /s /q build\Debug-Win32 build\Release-Win32 & ^
 rd /s /q build\Debug-x64 build\Release-x64

echo @@@BUILD_STEP partial_sdk@@@
call scons.bat --verbose --mode=nacl_extra_sdk platform=x86-%BITS% ^
 --download extra_sdk_update_header install_libpthread extra_sdk_update
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP gyp_compile@@@
call vcvarsall.bat x86 && call devenv.com build\all.sln /build %GYPMODE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP gyp_tests@@@
python_slave.exe trusted_test.py --config %GYPMODE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo on
echo @@@BUILD_STEP scons_compile@@@
call vcvarsall.bat %VCBITS% && call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP small_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc small_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP medium_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc medium_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP large_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc large_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP chrome_browser_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc SILENT=1 platform=x86-%BITS% ^
 chrome_browser_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@BUILD_FAILED@@@)

exit /b %RETCODE%
