#!/bin/sh
echo DEPRECATED gsutil -h "Cache-Control: no-cache" cp -a public-read naclsdk_manifest.json gs://nativeclient-mirror/nacl/nacl_sdk/naclsdk_manifest.json
gsutil -h "Cache-Control: no-cache" cp -a public-read naclsdk_manifest0.json gs://nativeclient-mirror/nacl/nacl_sdk/naclsdk_manifest0.json
gsutil -h "Cache-Control: no-cache" cp -a public-read naclsdk_manifest2.json gs://nativeclient-mirror/nacl/nacl_sdk/naclsdk_manifest2.json
