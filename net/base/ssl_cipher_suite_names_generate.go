// This program reads in the contents of [1] from /tmp/tls-parameters.xml and
// writes out a compact form the ciphersuite information found there in.
// It's used to generate the tables in net/base/ssl_cipher_suite_names.cc
//
// [1] http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
package main

import (
	"fmt"
	"os"
	"sort"
	"strings"
	"xml"
)

// Structures for parsing the XML

type TLSRegistry struct {
	Registry []Registry
}

type Registry struct {
	Id     string "attr"
	Title  string
	Record []Record
}

type Record struct {
	Value       string
	Description string
}

type CipherSuite struct {
	value  uint16
	kx     string
	cipher string
	mac    string
}

func fromHex(c byte) int {
	if c >= '0' && c <= '9' {
		return int(c - '0')
	}
	if c >= 'a' && c <= 'f' {
		return int(c - 'a' + 10)
	}
	if c >= 'A' && c <= 'F' {
		return int(c - 'A' + 10)
	}
	panic("Bad char passed to fromHex")
}

type TLSValue struct {
	v    int
	name string
}

type TLSMapping []TLSValue

func (m TLSMapping) Len() int {
	return len(m)
}

func (m TLSMapping) Less(i, j int) bool {
	return m[i].v < m[j].v
}

func (m TLSMapping) Swap(i, j int) {
	m[i], m[j] = m[j], m[i]
}

func printDict(d map[string]int, name string) {
	a := make([]TLSValue, len(d))

	maxLen := 0
	i := 0
	for k, v := range d {
		if len(k) > maxLen {
			maxLen = len(k)
		}
		a[i].v = v
		a[i].name = k
		i++
	}

	sort.Sort(TLSMapping(a))

	fmt.Printf("static const struct {\n  char name[%d];\n} %s[%d] = {\n", maxLen+1, name, len(d))
	for _, m := range a {
		fmt.Printf("  {\"%s\"},  // %d\n", m.name, m.v)
	}

	fmt.Printf("};\n\n")
}

func parseCipherSuiteString(s string) (kx, cipher, mac string) {
	s = s[4:]
	i := strings.Index(s, "_WITH_")
	kx = s[0:i]
	s = s[i+6:]
	i = strings.LastIndex(s, "_")
	cipher = s[0:i]
	mac = s[i+1:]
	return
}

func main() {
	infile, err := os.Open("/tmp/tls-parameters.xml", os.O_RDONLY, 0)
	if err != nil {
		fmt.Printf("Cannot open input: %s\n", err)
		return
	}

	var input TLSRegistry
	err = xml.Unmarshal(infile, &input)
	if err != nil {
		fmt.Printf("Error parsing XML: %s\n", err)
		return
	}

	var cipherSuitesRegistry *Registry
	for _, r := range input.Registry {
		if r.Id == "tls-parameters-4" {
			cipherSuitesRegistry = &r
			break
		}
	}

	if cipherSuitesRegistry == nil {
		fmt.Printf("Didn't find tls-parameters-4 registry\n")
	}

	kxs := make(map[string]int)
	next_kx := 0
	ciphers := make(map[string]int)
	next_cipher := 0
	macs := make(map[string]int)
	next_mac := 0
	lastValue := uint16(0)

	fmt.Printf("struct CipherSuite {\n  uint16 cipher_suite, encoded;\n};\n\n")
	fmt.Printf("static const struct CipherSuite kCipherSuites[] = {\n")

	for _, r := range cipherSuitesRegistry.Record {
		if strings.Index(r.Description, "_WITH_") == -1 {
			continue
		}

		value := uint16(fromHex(r.Value[2])<<12 | fromHex(r.Value[3])<<8 | fromHex(r.Value[7])<<4 | fromHex(r.Value[8]))
		kx, cipher, mac := parseCipherSuiteString(r.Description)

		if value < lastValue {
			panic("Input isn't sorted")
		}
		lastValue = value

		var kx_n, cipher_n, mac_n int
		var ok bool

		if kx_n, ok = kxs[kx]; !ok {
			kxs[kx] = next_kx
			kx_n = next_kx
			next_kx++
		}
		if cipher_n, ok = ciphers[cipher]; !ok {
			ciphers[cipher] = next_cipher
			cipher_n = next_cipher
			next_cipher++
		}
		if mac_n, ok = macs[mac]; !ok {
			macs[mac] = next_mac
			mac_n = next_mac
			next_mac++
		}

		if kx_n > 32 || cipher_n > 15 || mac_n > 7 {
			panic("Need to shift bit boundaries")
		}

		encoded := (kx_n << 7) | (cipher_n << 3) | mac_n
		fmt.Printf("  {0x%x, 0x%x},  // %s\n", value, encoded, r.Description)
	}

	fmt.Printf("};\n\n")

	printDict(kxs, "kKeyExchangeNames")
	printDict(ciphers, "kCipherNames")
	printDict(macs, "kMacNames")
}
