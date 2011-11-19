!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "Sections.nsh"
!include "x64.nsh"

RequestExecutionLevel user
SetCompressor /solid lzma
SetCompressorDictSize 128
Name "Native Client SDK"
OutFile ../../nacl-sdk.exe

; The full SDK install name is generated from the version string.
!include sdk_install_name.nsh

Var SVV_CmdLineParameters
Var SVV_SelChangeInProgress

!define MUI_HEADERIMAGE
!define MUI_WELCOMEFINISHPAGE_BITMAP \
    "${NSISDIR}\Contrib\Graphics\Wizard\win.bmp"

!define MUI_WELCOMEPAGE_TITLE "Welcome to the Native Client SDK Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the \
installation of the Native Client SDK $\r$\n$\r$\nThe Native Client SDK \
includes a GNU toolchain adopted for Native Client use and some examples. You \
need Google Chrome to test the examples.$\r$\n$\r$\nYou will also need to \
install Python (please visit www.python.org/download)$\r$\n$\r$\n$_CLICK"

!define MUI_COMPONENTSPAGE_SMALLDESC

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_LINK \
    "Visit the Native Client site for news, FAQs and support"
!define MUI_FINISHPAGE_LINK_LOCATION \
    "http://code.google.com/chrome/nativeclient"

!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Show release notes"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION ShowReleaseNotes

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "" sec_Preinstall
  Push $R0
  CreateDirectory "$INSTDIR"
  ; Owner can do anything
  AccessControlW::GrantOnFile "$INSTDIR" "(S-1-3-0)" "FullAccess"
  ; Group can read
  AccessControlW::GrantOnFile "$INSTDIR" "(S-1-3-1)" "Traverse + GenericRead"
  ; "Everyone" can read too
  AccessControlW::GrantOnFile "$INSTDIR" "(S-1-1-0)" "Traverse + GenericRead"
  FileClose $R0
  Pop $R0
SectionEnd

; The SDK Section commands are in a generated file.
!include sdk_section.nsh

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT \
               ${NativeClientSDK} \
               "Native Client SDK - toolchain and examples"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
  ${GetParameters} $SVV_CmdLineParameters
  Push $R0
  ClearErrors
  ${GetOptions} $SVV_CmdLineParameters "/?" $R0
  IfErrors +1 HelpMessage
  ${GetOptions} $SVV_CmdLineParameters "--help" $R0
  IfErrors +3 +1
HelpMessage:
  MessageBox MB_OK "Recognized common options:$\n \
  /D=InstDir - use InstDir as target instead of usual $INSTDIR$\n \
  /NCRC - disables the CRC check$\n \
  /S - Silent install"
  Abort
  Pop $R0
FunctionEnd

Function .onSelChange
  ${If} $SVV_SelChangeInProgress == 0
    StrCpy $SVV_SelChangeInProgress 1
    Push $R0
    IntOp $R0 ${SF_SELECTED} | ${SF_BOLD}
    SectionSetFlags ${NativeClientSDK} $R0
    Pop $R0
    StrCpy $SVV_SelChangeInProgress 0
  ${EndIf}
FunctionEnd

Function ShowReleaseNotes
  ExecShell "open" \
      "http://code.google.com/chrome/nativeclient/docs/releasenotes.html"
FunctionEnd
