#!/bin/sh

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a set of test (end-entity, intermediate, root)
# certificates that can be used to test fetching of an intermediate via AIA.

try() {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

try /bin/sh -c "echo 01 > out/2048-sha256-root-serial"
touch out/2048-sha256-root-index.txt

# Generate the key
try openssl genrsa -out out/2048-sha256-root.key 2048

# Generate the root certificate
CA_COMMON_NAME="Test Root CA" \
  try openssl req \
    -new \
    -key out/2048-sha256-root.key \
    -out out/2048-sha256-root.req \
    -config ca.cnf

CA_COMMON_NAME="Test Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -in out/2048-sha256-root.req \
    -out out/2048-sha256-root.pem \
    -signkey out/2048-sha256-root.key \
    -extfile ca.cnf \
    -extensions ca_cert \
    -text

# Generate the leaf certificate requests
try openssl req \
  -new \
  -keyout out/expired_cert.key \
  -out out/expired_cert.req \
  -config ee.cnf

try openssl req \
  -new \
  -keyout out/ok_cert.key \
  -out out/ok_cert.req \
  -config ee.cnf

# Generate the leaf certificates
CA_COMMON_NAME="Test Root CA" \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 060101000000Z \
    -enddate 070101000000Z \
    -in out/expired_cert.req \
    -out out/expired_cert.pem \
    -config ca.cnf

CA_COMMON_NAME="Test Root CA" \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -days 3650 \
    -in out/ok_cert.req \
    -out out/ok_cert.pem \
    -config ca.cnf

CA_COMMON_NAME="Test Root CA" \
  try openssl ca \
    -batch \
    -extensions name_constraint_bad \
    -subj "/CN=Leaf certificate/" \
    -days 3650 \
    -in out/ok_cert.req \
    -out out/name_constraint_bad.pem \
    -config ca.cnf

CA_COMMON_NAME="Test Root CA" \
  try openssl ca \
    -batch \
    -extensions name_constraint_good \
    -subj "/CN=Leaf Certificate/" \
    -days 3650 \
    -in out/ok_cert.req \
    -out out/name_constraint_good.pem \
    -config ca.cnf

try /bin/sh -c "cat out/ok_cert.key out/ok_cert.pem \
    > ../certificates/ok_cert.pem"
try /bin/sh -c "cat out/expired_cert.key out/expired_cert.pem \
    > ../certificates/expired_cert.pem"
try /bin/sh -c "cat out/2048-sha256-root.key out/2048-sha256-root.pem \
    > ../certificates/root_ca_cert.pem"
try /bin/sh -c "cat out/ok_cert.key out/name_constraint_bad.pem \
    > ../certificates/name_constraint_bad.pem"
try /bin/sh -c "cat out/ok_cert.key out/name_constraint_good.pem \
    > ../certificates/name_constraint_good.pem"

# Now generate the one-off certs
## SHA-256 general test cert
try openssl req -x509 -days 3650 \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -sha256 \
    -out sha256.pem

## Self-signed cert for SPDY/QUIC/HTTP2 pooling testing
try openssl req -x509 -days 3650 -extensions req_spdy_pooling \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/spdy_pooling.pem

## SubjectAltName parsing
try openssl req -x509 -days 3650 -extensions req_san_sanity \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/subjectAltName_sanity_check.pem

## Punycode handling
SUBJECT_NAME="req_punycode_dn" \
  try openssl req -x509 -days 3650 -extensions req_punycode \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
     -out ../certificates/punycodetest.pem

# Regenerate CRLSets
## Block a leaf cert directly by SPKI
try python crlsetutil.py -o ../certificates/crlset_by_leaf_spki.raw \
<<CRLBYLEAFSPKI
{
  "BlockedBySPKI": ["../certificates/ok_cert.pem"]
}
CRLBYLEAFSPKI

## Block a leaf cert by issuer-hash-and-serial (ok_cert.pem == serial 2, by
## virtue of the serial file and ordering above.
try python crlsetutil.py -o ../certificates/crlset_by_root_serial.raw \
<<CRLBYROOTSERIAL
{
  "BlockedByHash": {
    "../certificates/root_ca_cert.pem": [2]
  }
}
CRLBYROOTSERIAL

## Block a leaf cert by issuer-hash-and-serial. However, this will be issued
## from an intermediate CA issued underneath a root.
try python crlsetutil.py -o ../certificates/crlset_by_intermediate_serial.raw \
<<CRLSETBYINTERMEDIATESERIAL
{
  "BlockedByHash": {
    "../certificates/quic_intermediate.crt": [3]
  }
}
CRLSETBYINTERMEDIATESERIAL
