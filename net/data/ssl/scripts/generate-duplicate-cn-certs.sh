#!/bin/sh

# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates two chains of test certificates:
#    1. A1 (end-entity) -> B (self-signed root)
#    2. A2 (end-entity) -> B (self-signed root)
#
# In which A1 and A2 share the same key, the same subject common name, but have
# distinct O values in their subjects.
#
# This is used to test that NSS can properly generate unique certificate
# nicknames for both certificates.

try () {
  echo "$@"
  $@ || exit 1
}

generate_key_command () {
  case "$1" in
    rsa)
      echo genrsa
      ;;
    *)
      exit 1
  esac
}

try rm -rf out
try mkdir out

echo Create the serial number and index files.
try echo 1 > out/B-serial
try touch out/B-index.txt

echo Generate the keys.
try openssl genrsa -out out/A.key 2048
try openssl genrsa -out out/B.key 2048

echo Generate the B CSR.
CA_COMMON_NAME="B Root CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  KEY_SIZE=2048 \
  ALGO=rsa \
  CERT_TYPE=root \
  TYPE=B CERTIFICATE=B \
  try openssl req \
    -new \
    -key out/B.key \
    -out out/B.csr \
    -config redundant-ca.cnf

echo B signs itself.
CA_COMMON_NAME="B Root CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  try openssl x509 \
    -req -days 3650 \
    -in out/B.csr \
    -extfile redundant-ca.cnf \
    -extensions ca_cert \
    -signkey out/B.key \
    -out out/B.pem

echo Generate the A1 end-entity CSR.
SUBJECT_NAME=req_duplicate_cn_1 \
  try openssl req \
    -new \
    -key out/A.key \
    -out out/A1.csr \
    -config ee.cnf

echo Generate the A2 end-entity CSR
SUBJECT_NAME=req_duplicate_cn_2 \
  try openssl req \
    -new \
    -key out/A.key \
    -out out/A2.csr \
    -config ee.cnf


echo B signs A1.
CA_COMMON_NAME="B CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  KEY_SIZE=2048 \
  ALGO=sha1 \
  CERT_TYPE=intermediate \
  TYPE=B CERTIFICATE=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A1.csr \
    -out out/A1.pem \
    -config redundant-ca.cnf

echo B signs A2.
CA_COMMON_NAME="B CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  KEY_SIZE=2048 \
  ALGO=sha1 \
  CERT_TYPE=intermediate \
  TYPE=B CERTIFICATE=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A2.csr \
    -out out/A2.pem \
    -config redundant-ca.cnf

echo Exporting the certificates to PKCS#12
try openssl pkcs12 \
  -export \
  -inkey out/A.key \
  -in out/A1.pem \
  -out ../certificates/duplicate_cn_1.p12 \
  -passout pass:chrome

try openssl pkcs12 \
  -export \
  -inkey out/A.key \
  -in out/A2.pem \
  -out ../certificates/duplicate_cn_2.p12 \
  -passout pass:chrome

cp out/A1.pem ../certificates/duplicate_cn_1.pem
cp out/A2.pem ../certificates/duplicate_cn_2.pem
