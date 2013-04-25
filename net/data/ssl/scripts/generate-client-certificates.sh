#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates certificates that can be used to test SSL client
# authentication. Outputs for automated tests are stored in
# net/data/ssl/certificates, but may be re-generated for manual testing.
#
# This script generates two chains of test client certificates:
#
#   1. A (end-entity) -> B -> C (self-signed root)
#   2. D (end-entity) -> E -> C (self-signed root)
#
# In which A, B, C, D, and E all have distinct keypairs. Both client
# certificates share the same root, but are issued by different
# intermediates. The names of these intermediates are hardcoded within
# unit tests, and thus should not be changed.

try () {
  echo "$@"
  $@ || exit 1
}

try rm -rf out
try mkdir out

echo Create the serial number files and indices.
serial = 100
for i in B C E
do
  try echo $serial > out/$i-serial
  serial=$(expr $serial + 1)
  touch out/$i-index.txt
  touch out/$i-index.txt.attr
done

echo Generate the keys.
for i in A B C D E
do
  try openssl genrsa -out out/$i.key 2048
done

echo Generate the C CSR
COMMON_NAME="C Root CA" \
  CA_DIR=out \
  ID=C \
  try openssl req \
    -new \
    -key out/C.key \
    -out out/C.csr \
    -config client-certs.cnf

echo C signs itself.
COMMON_NAME="C Root CA" \
  CA_DIR=out \
  ID=C \
  try openssl x509 \
    -req -days 3650 \
    -in out/C.csr \
    -extensions ca_cert \
    -signkey out/C.key \
    -out out/C.pem

echo Generate the intermediates
COMMON_NAME="B CA" \
  CA_DIR=out \
  ID=B \
  try openssl req \
    -new \
    -key out/B.key \
    -out out/B.csr \
    -config client-certs.cnf

COMMON_NAME="C CA" \
  CA_DIR=out \
  ID=C \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/B.csr \
    -out out/B.pem \
    -config client-certs.cnf

COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl req \
    -new \
    -key out/E.key \
    -out out/E.csr \
    -config client-certs.cnf

COMMON_NAME="C CA" \
  CA_DIR=out \
  ID=C \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/E.csr \
    -out out/E.pem \
    -config client-certs.cnf

echo Generate the leaf certs
for id in A D
do
  COMMON_NAME="Client Cert $id" \
  ID=$id \
  try openssl req \
    -new \
    -key out/$id.key \
    -out out/$id.csr \
    -config client-certs.cnf
done

echo B signs A
COMMON_NAME="B CA" \
  CA_DIR=out \
  ID=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A.csr \
    -out out/A.pem \
    -config client-certs.cnf

echo E signs D
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/D.csr \
    -out out/D.pem \
    -config client-certs.cnf

echo Package the client certs and private keys into PKCS12 files
# This is done for easily importing all of the certs needed for clients.
cat out/A.pem out/A.key out/B.pem out/C.pem > out/A-chain.pem
cat out/D.pem out/D.key out/E.pem out/C.pem > out/D-chain.pem

try openssl pkcs12 \
  -in out/A-chain.pem \
  -out client_1.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/D-chain.pem \
  -out client_2.p12 \
  -export \
  -passout pass:chrome

echo Package the client certs for unit tests
cp out/A.pem client_1.pem
cp out/A.key client_1.key
cp out/B.pem client_1_ca.pem

cp out/D.pem client_2.pem
cp out/D.key client_2.key
cp out/E.pem client_2_ca.pem
