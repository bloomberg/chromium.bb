// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program converts the information in
// transport_security_state_static.json and
// transport_security_state_static.certs into
// transport_security_state_static.h. The input files contain information about
// public key pinning and HTTPS-only sites that is compiled into Chromium.

// Run as:
// % go run transport_security_state_static_generate.go transport_security_state_static.json transport_security_state_static.certs
//
// It will write transport_security_state_static.h

package main

import (
	"bufio"
	"bytes"
	"crypto/sha1"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"os"
	"regexp"
	"strings"
)

// A pin represents an entry in transport_security_state_static.certs. It's a
// name associated with a SubjectPublicKeyInfo hash and, optionally, a
// certificate.
type pin struct {
	name         string
	cert         *x509.Certificate
	spkiHash     []byte
	spkiHashFunc string // i.e. "sha1"
}

// preloaded represents the information contained in the
// transport_security_state_static.json file. This structure and the two
// following are used by the "json" package to parse the file. See the comments
// in transport_security_state_static.json for details.
type preloaded struct {
	Pinsets []pinset `json:"pinsets"`
	Entries []hsts   `json:"entries"`
}

type pinset struct {
	Name    string   `json:"name"`
	Include []string `json:"static_spki_hashes"`
	Exclude []string `json:"bad_static_spki_hashes"`
}

type hsts struct {
	Name       string `json:"name"`
	Subdomains bool   `json:"include_subdomains"`
	Mode       string `json:"mode"`
	Pins       string `json:"pins"`
	SNIOnly    bool   `json:"snionly"`
}

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintf(os.Stderr, "Usage: %s <json file> <certificates file>\n", os.Args[0])
		os.Exit(1)
	}

	if err := process(os.Args[1], os.Args[2]); err != nil {
		fmt.Fprintf(os.Stderr, "Conversion failed: %s\n", err.Error())
		os.Exit(1)
	}
}

func process(jsonFileName, certsFileName string) error {
	jsonFile, err := os.Open(jsonFileName)
	if err != nil {
		return fmt.Errorf("failed to open input file: %s\n", err.Error())
	}
	defer jsonFile.Close()

	jsonBytes, err := removeComments(jsonFile)
	if err != nil {
		return fmt.Errorf("failed to remove comments from JSON: %s\n", err.Error())
	}

	var preloaded preloaded
	if err := json.Unmarshal(jsonBytes, &preloaded); err != nil {
		return fmt.Errorf("failed to parse JSON: %s\n", err.Error())
	}

	certsFile, err := os.Open(certsFileName)
	if err != nil {
		return fmt.Errorf("failed to open input file: %s\n", err.Error())
	}
	defer certsFile.Close()

	pins, err := parseCertsFile(certsFile)
	if err != nil {
		return fmt.Errorf("failed to parse certificates file: %s\n", err)
	}

	if err := checkDuplicatePins(pins); err != nil {
		return err
	}

	if err := checkCertsInPinsets(preloaded.Pinsets, pins); err != nil {
		return err
	}

	if err := checkNoopEntries(preloaded.Entries); err != nil {
		return err
	}

	if err := checkDuplicateEntries(preloaded.Entries); err != nil {
		return err
	}

	outFile, err := os.OpenFile("transport_security_state_static.h", os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		return err
	}
	defer outFile.Close()

	out := bufio.NewWriter(outFile)
	writeHeader(out)
	writeCertsOutput(out, pins)
	writeHSTSOutput(out, preloaded)
	writeFooter(out)
	out.Flush()

	return nil
}

var newLine = []byte("\n")
var startOfCert = []byte("-----BEGIN CERTIFICATE")
var endOfCert = []byte("-----END CERTIFICATE")
var startOfSHA1 = []byte("sha1/")

// nameRegexp matches valid pin names: an uppercase letter followed by zero or
// more letters and digits.
var nameRegexp = regexp.MustCompile("[A-Z][a-zA-Z0-9_]*")

// commentRegexp matches lines that optionally start with whitespace
// followed by "//".
var commentRegexp = regexp.MustCompile("^[ \t]*//")

// removeComments reads the contents of |r| and removes any lines beginning
// with optional whitespace followed by "//"
func removeComments(r io.Reader) ([]byte, error) {
	var buf bytes.Buffer
	in := bufio.NewReader(r)

	for {
		line, isPrefix, err := in.ReadLine()
		if isPrefix {
			return nil, errors.New("line too long in JSON")
		}
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
		if commentRegexp.Match(line) {
			continue
		}
		buf.Write(line)
		buf.Write(newLine)
	}

	return buf.Bytes(), nil
}

// parseCertsFile parses |inFile|, in the format of
// transport_security_state_static.certs. See the comments at the top of that
// file for details of the format.
func parseCertsFile(inFile io.Reader) ([]pin, error) {
	const (
		PRENAME  = iota
		POSTNAME = iota
		INCERT   = iota
	)

	in := bufio.NewReader(inFile)

	lineNo := 0
	var pemCert []byte
	state := PRENAME
	var name string
	var pins []pin

	for {
		lineNo++
		line, isPrefix, err := in.ReadLine()
		if isPrefix {
			return nil, fmt.Errorf("line %d is too long to process\n", lineNo)
		}
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("error reading from input: %s\n", err.Error())
		}

		if len(line) == 0 || line[0] == '#' {
			continue
		}

		switch state {
		case PRENAME:
			name = string(line)
			if !nameRegexp.MatchString(name) {
				return nil, fmt.Errorf("invalid name on line %d\n", lineNo)
			}
			state = POSTNAME
		case POSTNAME:
			switch {
			case bytes.HasPrefix(line, startOfSHA1):
				hash, err := base64.StdEncoding.DecodeString(string(line[len(startOfSHA1):]))
				if err != nil {
					return nil, fmt.Errorf("failed to decode hash on line %d: %s\n", lineNo, err)
				}
				if len(hash) != 20 {
					return nil, fmt.Errorf("bad SHA1 hash length on line %d: %s\n", lineNo, err)
				}
				pins = append(pins, pin{
					name:         name,
					spkiHashFunc: "sha1",
					spkiHash:     hash,
				})
				state = PRENAME
				continue
			case bytes.HasPrefix(line, startOfCert):
				pemCert = pemCert[:0]
				pemCert = append(pemCert, line...)
				pemCert = append(pemCert, '\n')
				state = INCERT
			default:
				return nil, fmt.Errorf("line %d, after a name, is not a hash nor a certificate\n", lineNo)
			}
		case INCERT:
			pemCert = append(pemCert, line...)
			pemCert = append(pemCert, '\n')
			if !bytes.HasPrefix(line, endOfCert) {
				continue
			}

			block, _ := pem.Decode(pemCert)
			if block == nil {
				return nil, fmt.Errorf("failed to decode certificate ending on line %d\n", lineNo)
			}
			cert, err := x509.ParseCertificate(block.Bytes)
			if err != nil {
				return nil, fmt.Errorf("failed to parse certificate ending on line %d: %s\n", lineNo, err.Error())
			}
			certName := cert.Subject.CommonName
			if len(certName) == 0 {
				certName = cert.Subject.Organization[0] + " " + cert.Subject.OrganizationalUnit[0]
			}
			if err := matchNames(certName, name); err != nil {
				return nil, fmt.Errorf("name failure on line %d: %s\n%s -> %s\n", lineNo, err, certName, name)
			}
			h := sha1.New()
			h.Write(cert.RawSubjectPublicKeyInfo)
			pins = append(pins, pin{
				name:         name,
				cert:         cert,
				spkiHashFunc: "sha1",
				spkiHash:     h.Sum(nil),
			})
			state = PRENAME
		}
	}

	return pins, nil
}

// matchNames returns true if the given pin name is a reasonable match for the
// given CN.
func matchNames(name, v string) error {
	words := strings.Split(name, " ")
	if len(words) == 0 {
		return errors.New("no words in certificate name")
	}
	firstWord := words[0]
	if strings.HasSuffix(firstWord, ",") {
		firstWord = firstWord[:len(firstWord)-1]
	}
	if strings.HasPrefix(firstWord, "*.") {
		firstWord = firstWord[2:]
	}
	if pos := strings.Index(firstWord, "."); pos != -1 {
		firstWord = firstWord[:pos]
	}
	if pos := strings.Index(firstWord, "-"); pos != -1 {
		firstWord = firstWord[:pos]
	}
	if len(firstWord) == 0 {
		return errors.New("first word of certificate name is empty")
	}
	firstWord = strings.ToLower(firstWord)
	lowerV := strings.ToLower(v)
	if !strings.HasPrefix(lowerV, firstWord) {
		return errors.New("the first word of the certificate name isn't a prefix of the variable name")
	}

	for i, word := range words {
		if word == "Class" && i+1 < len(words) {
			if strings.Index(v, word+words[i+1]) == -1 {
				return errors.New("class specification doesn't appear in the variable name")
			}
		} else if len(word) == 1 && word[0] >= '0' && word[0] <= '9' {
			if strings.Index(v, word) == -1 {
				return errors.New("number doesn't appear in the variable name")
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

// checkDuplicatePins returns an error if any pins have the same name or the same hash.
func checkDuplicatePins(pins []pin) error {
	seenNames := make(map[string]bool)
	seenHashes := make(map[string]string)

	for _, pin := range pins {
		if _, ok := seenNames[pin.name]; ok {
			return fmt.Errorf("duplicate name: %s", pin.name)
		}
		seenNames[pin.name] = true

		strHash := string(pin.spkiHash)
		if otherName, ok := seenHashes[strHash]; ok {
			return fmt.Errorf("duplicate hash for %s and %s", pin.name, otherName)
		}
		seenHashes[strHash] = pin.name
	}

	return nil
}

// checkCertsInPinsets returns an error if
//   a) unknown pins are mentioned in |pinsets|
//   b) unused pins are given in |pins|
//   c) a pinset name is used twice
func checkCertsInPinsets(pinsets []pinset, pins []pin) error {
	pinNames := make(map[string]bool)
	for _, pin := range pins {
		pinNames[pin.name] = true
	}

	usedPinNames := make(map[string]bool)
	pinsetNames := make(map[string]bool)

	for _, pinset := range pinsets {
		if _, ok := pinsetNames[pinset.Name]; ok {
			return fmt.Errorf("duplicate pinset name: %s", pinset.Name)
		}
		pinsetNames[pinset.Name] = true

		var allPinNames []string
		allPinNames = append(allPinNames, pinset.Include...)
		allPinNames = append(allPinNames, pinset.Exclude...)

		for _, pinName := range allPinNames {
			if _, ok := pinNames[pinName]; !ok {
				return fmt.Errorf("unknown pin: %s", pinName)
			}
			usedPinNames[pinName] = true
		}
	}

	for pinName := range pinNames {
		if _, ok := usedPinNames[pinName]; !ok {
			return fmt.Errorf("unused pin: %s", pinName)
		}
	}

	return nil
}

func checkNoopEntries(entries []hsts) error {
	for _, e := range entries {
		if len(e.Mode) == 0 && len(e.Pins) == 0 {
			switch e.Name {
			// This entry is deliberately used as an exclusion.
			case "learn.doubleclick.net":
				continue
			default:
				return errors.New("Entry for " + e.Name + " has no mode and no pins")
			}
		}
	}

	return nil
}

func checkDuplicateEntries(entries []hsts) error {
	seen := make(map[string]bool)

	for _, e := range entries {
		if _, ok := seen[e.Name]; ok {
			return errors.New("Duplicate entry for " + e.Name)
		}
		seen[e.Name] = true
	}

	return nil
}

func writeHeader(out *bufio.Writer) {
	out.WriteString(`// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is automatically generated by transport_security_state_static_generate.go

#ifndef NET_BASE_TRANSPORT_SECURITY_STATE_STATIC_H_
#define NET_BASE_TRANSPORT_SECURITY_STATE_STATIC_H_

`)

}

func writeFooter(out *bufio.Writer) {
	out.WriteString("#endif // NET_BASE_TRANSPORT_SECURITY_STATE_STATIC_H_\n")
}

func writeCertsOutput(out *bufio.Writer, pins []pin) {
	out.WriteString(`// These are SubjectPublicKeyInfo hashes for public key pinning. The
// hashes are base64 encoded, SHA1 digests.

`)

	for _, pin := range pins {
		fmt.Fprintf(out, "static const char kSPKIHash_%s[] =\n", pin.name)
		fmt.Fprintf(out, "    \"%s/%s\";\n\n", pin.spkiHashFunc, base64.StdEncoding.EncodeToString(pin.spkiHash))
	}
}

// uppercaseFirstLetter returns s with the first letter uppercased.
func uppercaseFirstLetter(s string) string {
	// We need to find the index of the second code-point, which may not be
	// one.
	for i := range s {
		if i == 0 {
			continue
		}
		return strings.ToUpper(s[:i]) + s[i:]
	}
	return strings.ToUpper(s)
}

func writeListOfPins(w io.Writer, name string, pinNames []string) {
	fmt.Fprintf(w, "static const char* const %s[] = {\n", name)
	for _, pinName := range pinNames {
		fmt.Fprintf(w, "  kSPKIHash_%s,\n", pinName)
	}
	fmt.Fprintf(w, "  NULL,\n};\n")
}

// toDNS returns a string converts the domain name |s| into C-escaped,
// length-prefixed form and also returns the length of the interpreted string.
// i.e. for an input "example.com" it will return "\\007example\\003com", 13.
func toDNS(s string) (string, int) {
	labels := strings.Split(s, ".")

	var name string
	var l int
	for _, label := range labels {
		if len(label) > 63 {
			panic("DNS label too long")
		}
		name += fmt.Sprintf("\\%03o", len(label))
		name += label
		l += len(label) + 1
	}
	l += 1 // For the length of the root label.

	return name, l
}

// domainConstant converts the domain name |s| into a string of the form
// "DOMAIN_" + uppercase last two labels.
func domainConstant(s string) string {
	labels := strings.Split(s, ".")
	gtld := strings.ToUpper(labels[len(labels)-1])
	domain := strings.Replace(strings.ToUpper(labels[len(labels)-2]), "-", "_", -1)

	return fmt.Sprintf("DOMAIN_%s_%s", domain, gtld)
}

func writeHSTSEntry(out *bufio.Writer, entry hsts) {
	dnsName, dnsLen := toDNS(entry.Name)
	domain := "DOMAIN_NOT_PINNED"
	pinsetName := "kNoPins"
	if len(entry.Pins) > 0 {
		pinsetName = fmt.Sprintf("k%sPins", uppercaseFirstLetter(entry.Pins))
		domain = domainConstant(entry.Name)
	}
	fmt.Fprintf(out, "  {%d, %t, \"%s\", %t, %s, %s },\n", dnsLen, entry.Subdomains, dnsName, entry.Mode == "force-https", pinsetName, domain)
}

func writeHSTSOutput(out *bufio.Writer, hsts preloaded) error {
	out.WriteString(`// The following is static data describing the hosts that are hardcoded with
// certificate pins or HSTS information.

// kNoRejectedPublicKeys is a placeholder for when no public keys are rejected.
static const char* const kNoRejectedPublicKeys[] = {
  NULL,
};

`)

	for _, pinset := range hsts.Pinsets {
		name := uppercaseFirstLetter(pinset.Name)
		acceptableListName := fmt.Sprintf("k%sAcceptableCerts", name)
		writeListOfPins(out, acceptableListName, pinset.Include)

		rejectedListName := "kNoRejectedPublicKeys"
		if len(pinset.Exclude) > 0 {
			rejectedListName = fmt.Sprintf("k%sRejectedCerts", name)
			writeListOfPins(out, rejectedListName, pinset.Exclude)
		}
		fmt.Fprintf(out, `#define k%sPins { \
  %s, \
  %s, \
}

`, name, acceptableListName, rejectedListName)
	}

	out.WriteString(`#define kNoPins {\
  NULL, NULL, \
}

static const struct HSTSPreload kPreloadedSTS[] = {
`)

	for _, entry := range hsts.Entries {
		if entry.SNIOnly {
			continue
		}
		writeHSTSEntry(out, entry)
	}

	out.WriteString(`};
static const size_t kNumPreloadedSTS = ARRAYSIZE_UNSAFE(kPreloadedSTS);

static const struct HSTSPreload kPreloadedSNISTS[] = {
`)

	for _, entry := range hsts.Entries {
		if !entry.SNIOnly {
			continue
		}
		writeHSTSEntry(out, entry)
	}

	out.WriteString(`};
static const size_t kNumPreloadedSNISTS = ARRAYSIZE_UNSAFE(kPreloadedSNISTS);

`)

	return nil
}
