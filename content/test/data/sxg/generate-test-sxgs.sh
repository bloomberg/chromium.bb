#!/bin/sh

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

for cmd in gen-signedexchange gen-certurl dump-signedexchange; do
    if ! command -v $cmd > /dev/null 2>&1; then
        echo "$cmd is not installed. Please run:"
        echo "  go get -u github.com/WICG/webpackage/go/signedexchange/cmd/..."
        exit 1
    fi
done

tmpdir=$(mktemp -d)
sctdir=$tmpdir/scts
mkdir $sctdir

# Make dummy OCSP and SCT data for cbor certificate chains.
echo -n OCSP >$tmpdir/ocsp; echo -n SCT >$sctdir/dummy.sct

# Generate the certificate chain of "*.example.org".
gen-certurl -pem prime256v1-sha256.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir > test.example.org.public.pem.cbor

# Generate the certificate chain of "*.example.org", without
# CanSignHttpExchangesDraft extension.
gen-certurl -pem prime256v1-sha256-noext.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir > test.example.org-noext.public.pem.cbor

# Generate the signed exchange file.
gen-signedexchange \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date 2018-03-12T05:53:20Z \
  -o test.example.org_test.sxg \
  -miRecordSize 100

# Generate the signed exchange file with noext certificate
gen-signedexchange \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256-noext.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date 2018-03-12T05:53:20Z \
  -o test.example.org_noext_test.sxg \
  -miRecordSize 100

# Generate the signed exchange file with invalid URL.
gen-signedexchange \
  -uri https://test.example.com/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.com/resource.validity.msg \
  -privateKey prime256v1.key \
  -date 2018-03-12T05:53:20Z \
  -o test.example.com_invalid_test.sxg \
  -miRecordSize 100

# Generate the signed exchange for a plain text file.
gen-signedexchange \
  -uri https://test.example.org/hello.txt \
  -status 200 \
  -content hello.txt \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -responseHeader 'Content-Type: text/plain; charset=iso-8859-1' \
  -date 2018-03-12T05:53:20Z \
  -o test.example.org_hello.txt.sxg

echo "Update the test signatures in "
echo "signed_exchange_signature_verifier_unittest.cc with the followings:"
echo "===="

sed -ne '/-BEGIN PRIVATE KEY-/,/-END PRIVATE KEY-/p' \
  ../../../../net/data/ssl/certificates/wildcard.pem \
  > $tmpdir/wildcard_example.org.private.pem
sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' \
  ../../../../net/data/ssl/certificates/wildcard.pem \
  > $tmpdir/wildcard_example.org.public.pem
gen-signedexchange \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate $tmpdir/wildcard_example.org.public.pem \
  -privateKey $tmpdir/wildcard_example.org.private.pem \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg

echo -n 'constexpr char kSignatureHeaderRSA[] = R"('
dump-signedexchange -i $tmpdir/out.htxg | \
    sed -n 's/^signature: //p' | \
    tr -d '\n'
echo ')";'

gen-signedexchange \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate ./prime256v1-sha256.public.pem \
  -privateKey ./prime256v1.key \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg

echo -n 'constexpr char kSignatureHeaderECDSAP256[] = R"('
dump-signedexchange -i $tmpdir/out.htxg | \
    sed -n 's/^signature: //p' | \
    tr -d '\n'
echo ')";'

gen-signedexchange \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate ./secp384r1-sha256.public.pem \
  -privateKey ./secp384r1.key \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg

echo -n 'constexpr char kSignatureHeaderECDSAP384[] = R"('
dump-signedexchange -i $tmpdir/out.htxg | \
    sed -n 's/^signature: //p' | \
    tr -d '\n'
echo ')";'

echo "===="

rm -fr $tmpdir
