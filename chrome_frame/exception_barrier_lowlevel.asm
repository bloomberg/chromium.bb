; Copyright (c) 2010 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.
;
; Tag the exception handler as an SEH handler in case the executable
; is linked with /SAFESEH (which is the default).
;
; MASM 8.0 inserts an additional leading underscore in front of names
; and this is an attempted fix until we understand why.
; MASM 10.0 fixed this.
IF @version LT 800 OR @version GE 1000
_ExceptionBarrierHandler PROTO
.SAFESEH _ExceptionBarrierHandler
_ExceptionBarrierReportOnlyModuleHandler PROTO
.SAFESEH _ExceptionBarrierReportOnlyModuleHandler
_ExceptionBarrierCallCustomHandler PROTO
.SAFESEH _ExceptionBarrierCallCustomHandler
ELSE
ExceptionBarrierHandler PROTO
.SAFESEH ExceptionBarrierHandler
ExceptionBarrierReportOnlyModuleHandler PROTO
.SAFESEH ExceptionBarrierReportOnlyModuleHandler
ExceptionBarrierCallCustomHandler PROTO
.SAFESEH ExceptionBarrierCallCustomHandler
ENDIF

.586
.MODEL FLAT, STDCALL
ASSUME FS:NOTHING
.CODE

; extern "C" void WINAPI RegisterExceptionRecord(
;                          EXCEPTION_REGISTRATION *registration,
;                          ExceptionHandlerFunc func);
RegisterExceptionRecord PROC registration:DWORD, func:DWORD
OPTION PROLOGUE:None
OPTION EPILOGUE:None
  mov   edx, DWORD PTR [esp + 4]  ; edx is registration
  mov   eax, DWORD PTR [esp + 8] ; eax is func
  mov   DWORD PTR [edx + 4], eax
  mov   eax, FS:[0]
  mov   DWORD PTR [edx], eax
  mov   FS:[0], edx
  ret   8

RegisterExceptionRecord ENDP

; extern "C" void UnregisterExceptionRecord(
;                           EXCEPTION_REGISTRATION *registration);
UnregisterExceptionRecord PROC registration:DWORD
OPTION PROLOGUE:None
OPTION EPILOGUE:None

  mov   edx, DWORD PTR [esp + 4]
  mov   eax, [edx]
  mov   FS:[0], eax
  ret   4

UnregisterExceptionRecord ENDP

END
