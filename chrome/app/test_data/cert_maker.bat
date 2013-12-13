REM Copyright 2013 The Chromium Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

del /q certificates\*

REM Create a Cert Authority.
makecert -r -pe -n "CN=CertAuthority" -ss CA -sr CurrentUser ^
         -a sha256 -cy authority -sky signature ^
         -sv certificates\AuthorityCert.pvk certificates\AuthorityCert.cer

REM Create a second Cert Authority (used to test cert pinning).
makecert -r -pe -n "CN=OtherCertAuthority" -ss CA -sr CurrentUser ^
         -a sha256 -cy authority -sky signature ^
         -sv certificates\OtherAuthorityCert.pvk ^
         	 certificates\OtherAuthorityCert.cer

REM Create a self-signed cert.
makecert -r -pe -n "CN=SelfSigned" -ss CA -sr CurrentUser ^
         -a sha256 -cy authority -sky signature ^
         -sv certificates\SelfSigned.pvk certificates\SelfSigned.cer

pvk2pfx -pvk certificates\SelfSigned.pvk -spc certificates\SelfSigned.cer ^
    -pfx certificates\SelfSigned.pfx

signtool sign /v /f certificates\SelfSigned.pfx dlls\self_signed.dll

REM Create a signing cert from our first CA, with proper company name.
makecert -pe -n "CN=Google Inc, OU=Chrome Testers" -a sha256 -cy end ^
         -sky signature ^
         -ic certificates\AuthorityCert.cer -iv certificates\AuthorityCert.pvk ^
         -sv certificates\ValidCert.pvk certificates\ValidCert.cer

pvk2pfx -pvk certificates\ValidCert.pvk -spc certificates\ValidCert.cer ^
    -pfx certificates\ValidCert.pfx

signtool sign /v /f certificates\ValidCert.pfx dlls\valid_sig.dll

REM Create a signing cert from our first CA, with wrong company name.
makecert -pe -n "CN=NotGoogle Inc, OU=Chrome Testers" -a sha256 -cy end ^
         -sky signature ^
         -ic certificates\AuthorityCert.cer -iv certificates\AuthorityCert.pvk ^
         -sv certificates\NotGoogleCert.pvk certificates\NotGoogleCert.cer

pvk2pfx -pvk certificates\NotGoogleCert.pvk ^
		-spc certificates\NotGoogleCert.cer  -pfx certificates\NotGoogleCert.pfx

signtool sign /v /f certificates\NotGoogleCert.pfx dlls\not_google.dll

REM Create a signing cert from the other CA.
makecert -pe -n "CN=Google Inc, OU=Other Chrome Testers" -a sha256 -cy end ^
         -sky signature ^
         -ic certificates\OtherAuthorityCert.cer ^
         -iv certificates\OtherAuthorityCert.pvk ^
         -sv certificates\DifferentHash.pvk certificates\DifferentHash.cer

pvk2pfx -pvk certificates\DifferentHash.pvk ^
    -spc certificates\DifferentHash.cer -pfx certificates\DifferentHash.pfx

signtool sign /v /f certificates\DifferentHash.pfx dlls\different_hash.dll

REM Create an expired signing cert from our first CA.
makecert -pe -n "CN=Google Inc, OU=Chrome Testers" -a sha256 -cy end ^
         -sky signature ^
         -ic certificates\AuthorityCert.cer -iv certificates\AuthorityCert.pvk ^
         -e 12/31/2012 ^
         -sv certificates\ExpiredCert.pvk certificates\ExpiredCert.cer

pvk2pfx -pvk certificates\ExpiredCert.pvk -spc certificates\ExpiredCert.cer ^
    -pfx certificates\ExpiredCert.pfx

signtool sign /v /f certificates\ExpiredCert.pfx dlls\expired.dll

del /q certificates\*.pvk
del /q certificates\*.pfx
