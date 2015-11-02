# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

KEYCHAIN_PASSWORD=g0atMaster
# This must match the commonName in codesign.cfg.
KEYCHAIN_IDENTITY=untrusted@goat.local

executable32: src.c
	clang -m32 -o $@ $^

executable64: src.c
	clang -m64 -o $@ $^

executablefat: executable32 executable64
	lipo -create -output $@ $^

lib32.dylib: src.c
	clang -m32 -shared -o $@ $^

lib64.dylib: src.c
	clang -m64 -shared -o $@ $^

libfat.dylib: lib64.dylib lib32.dylib
	lipo -create -output $@ $^

codesign.key:
	openssl genrsa -out $@ 2048

codesign.csr: codesign.key codesign.cfg
	openssl req -new -key $< -out $@ -config codesign.cfg

codesign.crt: codesign.csr codesign.key codesign.cfg
	openssl x509 -req -signkey codesign.key -sha256 \
		-extfile codesign.cfg -extensions req_attrs -in $< -out $@

codesign.keychain: codesign.key codesign.crt
	security create-keychain -p $(KEYCHAIN_PASSWORD) $(PWD)/$@
	security unlock-keychain -p $(KEYCHAIN_PASSWORD) $(PWD)/$@
	certtool i ./codesign.crt k=$(PWD)/$@ r=./codesign.key

signedexecutable32: executable32 codesign.keychain
	cp $< $@
	security unlock-keychain -p $(KEYCHAIN_PASSWORD) \
		$(PWD)/codesign.keychain
	codesign -s $(KEYCHAIN_IDENTITY) --keychain $(PWD)/codesign.keychain $@

libsigned64.dylib: lib64.dylib codesign.keychain
	cp $< $@
	security unlock-keychain -p $(KEYCHAIN_PASSWORD) \
		$(PWD)/codesign.keychain
	codesign -s $(KEYCHAIN_IDENTITY) --keychain $(PWD)/codesign.keychain $@

signedexecutablefat: executablefat codesign.keychain
	cp $< $@
	security unlock-keychain -p $(KEYCHAIN_PASSWORD) \
		$(PWD)/codesign.keychain
	codesign -s $(KEYCHAIN_IDENTITY) --keychain $(PWD)/codesign.keychain \
		$@ --all-architectures

.PHONY: test-bundle.app
test-bundle.app: signedexecutablefat libsigned64.dylib executable32
	ditto base-bundle.app $@
	ditto $< $@/Contents/MacOS/test-bundle
	ditto $(word 2,$^) $@/Contents/Frameworks/$(word 2,$^)
	ditto $(word 3,$^) $@/Contents/Resources/$(word 3,$^)
	security unlock-keychain -p $(KEYCHAIN_PASSWORD) \
		$(PWD)/codesign.keychain
	codesign -f -s $(KEYCHAIN_IDENTITY) --keychain $(PWD)/codesign.keychain \
		$@ --all-architectures --resource-rules ResourceRules

.PHONY: modified-bundle.app
modified-bundle.app: test-bundle.app lib32.dylib executable64
	ditto $< $@
	touch $@/Contents/Resources/codesign.cfg
	ditto $(word 2,$^) $@/Contents/Frameworks/libsigned64.dylib
	ditto $(word 3,$^) $@/Contents/Resources/executable32
	echo "foo" >> $@/Contents/Resources/Base.lproj/MainMenu.nib
	security unlock-keychain -p $(KEYCHAIN_PASSWORD) \
		$(PWD)/codesign.keychain
	codesign -f -s $(KEYCHAIN_IDENTITY) --keychain $(PWD)/codesign.keychain \
		$@/Contents/Resources/Base.lproj/MainMenu.nib

.PHONY: modified-bundle-and-exec.app
modified-bundle-and-exec.app: test-bundle.app lib32.dylib executable64
	ditto $< $@
	touch $@/Contents/Resources/codesign.cfg
	ditto $(word 2,$^) $@/Contents/Frameworks/libsigned64.dylib
	ditto $(word 3,$^) $@/Contents/Resources/executable32
	printf '\x31' | dd bs=1 seek=8097 count=1 conv=notrunc \
	of=$@/Contents/MacOS/test-bundle

.PHONY: modified-main-exec32.app
modified-main-exec32.app: test-bundle.app
	ditto $< $@
	printf '\x31' | dd bs=1 seek=8097 count=1 conv=notrunc \
										 of=$@/Contents/MacOS/test-bundle

.PHONY: modified-main-exec64.app
modified-main-exec64.app: test-bundle.app
	ditto $< $@
	printf '\x31' | dd bs=1 seek=24448 count=1 conv=notrunc \
	of=$@/Contents/MacOS/test-bundle
