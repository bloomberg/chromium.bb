@echo off

:: This batch file assumes that the correct version of python can be found in
:: the current directory, and that you have Visual Studio 8 installed in the
:: default location. It will try to find Visual Studio in the default
:: installation paths for x86 and x64 versions of windows as well as through
:: the PATH environment variable.

setlocal
IF EXIST "%ProgramFiles(x86)%\Microsoft Visual Studio 8\VC\bin\vcvars32.bat" (
  CALL "%ProgramFiles(x86)%\Microsoft Visual Studio 8\VC\bin\vcvars32.bat"
) ELSE IF EXIST "%ProgramFiles%\Microsoft Visual Studio 8\VC\bin\vcvars32.bat" (
  CALL "%ProgramFiles%\Microsoft Visual Studio 8\VC\bin\vcvars32.bat"
) ELSE (
  :: See "HELP CALL" for information on how to use %~$PATH:1 to find a file in
  :: the PATH.
  CALL :FIND_IN_PATH "vcvars32.bat"
)

:: If vcvasr32.bat cannot be found or there was a problem, stop execution.
IF %ERRORLEVEL%==1 GOTO :EOF
python "%~dp0chrome-update.py" %*
GOTO :EOF

:FIND_IN_PATH
  :: %~$PATH:1 works like "which" on linux; use it to see if the file exists and
  :: call it if found. If it cannot be found print an error and set errorlevel
  IF EXIST "%~$PATH:1" (
    CALL "%~$PATH:1"
  ) ELSE (
    ECHO Cannot find vcvars32.bat! (Do you have Visual Studio in your PATH?)
    SET ERRORLEVEL=1
  )
  GOTO :EOF
