#!/bin/bash
# Copyright 2019 Google, LLC
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -x -e

export GOCACHE="$(pwd)/cache/go_cache"
export GOPATH="$(pwd)/cache/gopath"
export GOROOT="$(pwd)/go/go"

cd skia

# Build task drivers from the infra repo.
export GOBIN="${1}"
git init
git remote add origin https://skia.googlesource.com/skia.git
git add .
git commit -a -m "dummy commit to make go modules work"
export GOFLAGS="-mod=readonly"
go mod download
go install -v go.skia.org/infra/infra/bots/task_drivers/build_push_docker_image
go install -v go.skia.org/infra/infra/bots/task_drivers/push_apps_from_skia_image
go install -v go.skia.org/infra/infra/bots/task_drivers/push_apps_from_skia_wasm_images
go install -v go.skia.org/infra/infra/bots/task_drivers/push_apps_from_wasm_image
go install -v go.skia.org/infra/infra/bots/task_drivers/update_go_deps

# Build task drivers from this repo.
task_drivers_dir=infra/bots/task_drivers
for td in $(cd ${task_drivers_dir} && ls); do
  CGO_ENABLED=0 GOARCH=amd64 GOOS=linux go build -o ${1}/${td} ${task_drivers_dir}/${td}/${td}.go
done
