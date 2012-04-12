// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// public_key_hashes_check.go runs tests on public_key_hashes.h. It's not run
// automatically, but rather as part of the process of manually updating
// public_key_hashes.h
//
// It verifies that each hash in the file is correct given the preceeding
// certificate and that the name of the variable matches the name given in the
// certifiate.
//
// Compile and run with:
// % cd net/base
// % 6g public_key_hashes_check.go && 6l public_key_hashes_check.6 && ./6.out ./public_key_hashes.h

package main

import (
	"bufio"
	"bytes"
	"crypto/sha1"
	"crypto/x509"
	"encoding/base64"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
)

const (
	PRECERT  = iota
	INCERT   = iota
	POSTCERT = iota
	POSTDECL = iota
)

var newLine = []byte("\n")
var startOfCert = []byte("-----BEGIN CERTIFICATE")
var endOfCert = []byte("-----END CERTIFICATE")
var hashDecl = []byte("static const char kSPKIHash_")

// matchNames returns true if the given variable name is a reasonable match for
// the given CN.
func matchNames(name, v string) error {
	words := strings.Split(name, " ")
	if len(words) == 0 {
		return errors.New("No words in certificate name")
	}
	firstWord := words[0]
	if strings.HasSuffix(firstWord, ",") {
		firstWord = firstWord[:len(firstWord)-1]
	}
	if pos := strings.Index(firstWord, "."); pos != -1 {
		firstWord = firstWord[:pos]
	}
	if pos := strings.Index(firstWord, "-"); pos != -1 {
		firstWord = firstWord[:pos]
	}
	if !strings.HasPrefix(v, firstWord) {
		return errors.New("The first word of the certificate name isn't a prefix of the variable name")
	}

	for i, word := range words {
		if word == "Class" && i+1 < len(words) {
			if strings.Index(v, word+words[i+1]) == -1 {
				return errors.New("Class specification doesn't appear in the variable name")
			}
		} else if len(word) == 1 && word[0] >= '0' && word[0] <= '9' {
			if strings.Index(v, word) == -1 {
				return errors.New("Number doesn't appear in the variable name")
			}
		} else if isImportantWordInCertificateName(word) {
			if strings.Index(v, word) == -1 {
				return errors.New(word + " doesn't appear in the variable name")
			}
		}
	}

	return nil
}

// isImportantWordInCertificateName returns true if w must be found in any
// corresponding variable name.
func isImportantWordInCertificateName(w string) bool {
	switch w {
	case "Universal", "Global", "EV", "G1", "G2", "G3", "G4", "G5":
		return true
	}
	return false
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintf(os.Stderr, "Usage: %s <public_key_hashes.h>\n", os.Args[0])
		return
	}

	inFile, err := os.Open(os.Args[1])
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to open input file: %s\n", err.Error())
		return
	}

	in := bufio.NewReader(inFile)
	defer inFile.Close()

	lineNo := 0
	var cert []byte
	state := PRECERT
	var x509Cert *x509.Certificate
	seenHashes := make(map[string]bool)

	for {
		lineNo++
		line, isPrefix, err := in.ReadLine()
		if isPrefix {
			fmt.Fprintf(os.Stderr, "Line %d is too long to process\n", lineNo)
			return
		}
		if err == io.EOF {
			return
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error reading from input: %s\n", err.Error())
			return
		}

		switch state {
		case INCERT:
			cert = append(cert, line...)
			cert = append(cert, newLine...)
		case POSTDECL:
			trimmed := bytes.TrimSpace(line)
			if len(trimmed) < 8 || !bytes.HasPrefix(trimmed, []byte("\"sha1/")) {
				fmt.Fprintf(os.Stderr, "Line %d is immediately after a declation, but failed to find a hash on it\n", lineNo)
				return
			}
			trimmed = trimmed[6 : len(trimmed)-2]
			h := sha1.New()
			h.Write(x509Cert.RawSubjectPublicKeyInfo)
			shouldBe := base64.StdEncoding.EncodeToString(h.Sum(nil))
			if shouldBe != string(trimmed) {
				fmt.Fprintf(os.Stderr, "Line %d: hash should be %s, but found %s\n", lineNo, shouldBe, trimmed)
				return
			}
			if _, ok := seenHashes[shouldBe]; ok {
				fmt.Fprintf(os.Stderr, "Line %d: duplicated hash\n", lineNo)
				return
			}
			seenHashes[shouldBe] = true
			state = PRECERT
			x509Cert = nil
		}

		if bytes.HasPrefix(line, startOfCert) {
			switch state {
			case PRECERT:
				cert = append(cert, line...)
				cert = append(cert, newLine...)
				state = INCERT
			case INCERT:
				fmt.Fprintf(os.Stderr, "Found start of certificate while in certificate at line %d\n", lineNo)
				return
			case POSTCERT:
				fmt.Fprintf(os.Stderr, "Found start of certificate while while looking for hash at line %d\n", lineNo)
				return
			case POSTDECL:
				fmt.Fprintf(os.Stderr, "Found start of certificate while while looking for hash string at line %d\n", lineNo)
				return
			default:
				panic("bad state")
			}
		} else if bytes.HasPrefix(line, endOfCert) {
			switch state {
			case PRECERT:
				fmt.Fprintf(os.Stderr, "Found end of certificate without certificate at line %d\n", lineNo)
				return
			case INCERT:
				block, _ := pem.Decode(cert)
				if block == nil {
					fmt.Fprintf(os.Stderr, "Failed to decode certificate ending on line %d\n", lineNo)
					return
				}
				if x509Cert, err = x509.ParseCertificate(block.Bytes); err != nil {
					fmt.Fprintf(os.Stderr, "Failed to parse certificate ending on line %d: %s\n", lineNo, err.Error())
					return
				}
				cert = nil
				state = POSTCERT
			case POSTCERT:
				fmt.Fprintf(os.Stderr, "Found end of certificate while while looking for hash at line %d\n", lineNo)
				return
			case POSTDECL:
				fmt.Fprintf(os.Stderr, "Found end of certificate while while looking for hash string at line %d\n", lineNo)
				return
			default:
				panic("bad state")
			}
		} else if bytes.HasPrefix(line, hashDecl) {
			switch state {
			case PRECERT:
				// No problem here. Not all declations need a certificate
			case INCERT:
				fmt.Fprintf(os.Stderr, "Found declation at line %d, but missed the end of the certificate\n", lineNo)
				return
			case POSTCERT:
				varName := line[len(hashDecl):]
				for i, c := range varName {
					if c == '[' {
						varName = varName[:i]
						break
					}
				}
				name := x509Cert.Subject.CommonName
				if len(name) == 0 {
					name = x509Cert.Subject.Organization[0] + " " + x509Cert.Subject.OrganizationalUnit[0]
				}
				if err := matchNames(name, string(varName)); err != nil {
					fmt.Fprintf(os.Stderr, "Name failure on line %d: %s\n%s -> %s\n", lineNo, err, name, varName)
					return
				}

				state = POSTDECL
			case POSTDECL:
				fmt.Fprintf(os.Stderr, "Found declation at line %d, but missed the hash value of the previous one\n", lineNo)
				return
			default:
				panic("bad state")
			}
		}
	}
}
