#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates certificates for the unittests in
# net/base/client_cert_store_unittest.cc. The output files are versioned in
# net/data/ssl/certificates (client_1.pem, client_2.pem).

try () {
  echo "$@"
  $@ || exit 1
}

# For each authority below a root ca certificate and one client certificate will
# be created.
authorities="1 2"

try rm -rf out
try mkdir out

for id in $authorities
do
  # Generate a private key for the root cert.
  try openssl genrsa -out out/root_$id.key 2048

  # Create a certificate signing request for the root cert.
  ID=$id \
  DISTINGUISHED_NAME=ca_dn \
  try openssl req \
    -new \
    -key out/root_$id.key \
    -out out/root_$id.csr \
    -config client_authentication.cnf

  # Sign the root cert.
  ID=$id \
  DISTINGUISHED_NAME=ca_dn \
  try openssl x509 \
    -req -days 3650 \
    -in out/root_$id.csr \
    -signkey out/root_$id.key \
    -text \
    -out out/root_$id.pem
    -config client_authentication.cnf

  # Generate a private key for the client.
  try openssl genrsa -out out/client_$id.key 2048

  # Create a certificate signing request for the client cert.
  ID=$id \
  DISTINGUISHED_NAME=client_dn \
  try openssl req \
    -new \
    -key out/client_$id.key \
    -out out/client_$id.csr \
    -config client_authentication.cnf

  try touch out/$id-index.txt
  try echo 1 > out/$id-serial

  ID=$id \
  DISTINGUISHED_NAME=client_dn \
  try openssl ca \
    -batch \
    -in out/client_$id.csr \
    -cert out/root_$id.pem \
    -keyfile out/root_$id.key \
    -out out/client_$id.pem \
    -config client_authentication.cnf

  # Package the client cert and private key into a pkcs12 file.
  try openssl pkcs12 \
    -inkey out/client_$id.key \
    -in out/client_$id.pem \
    -out out/client_$id.p12 \
    -export \
    -passout pass:chrome
done
