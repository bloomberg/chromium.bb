DOC_MODULE=fontconfig
DOC2HTML=docbook2html -u
DOC2TXT=docbook2txt

TXT=fontconfig-user.txt fontconfig-devel.txt
HTML=fontconfig-user.html fontconfig-devel.html
SGML=fontconfig-user.sgml fontconfig-devel.sgml

DOCDIR=@DOCDIR@
TARGET_DIR=$(DOCDIR)

SUFFIXES=.sgml .txt .html

.sgml.html:
	$(RM) $@
	$(DOC2HTML) $*.sgml
	$(RM) index.html

.sgml.txt:
	$(RM) $@
	$(DOC2TXT) $*.sgml

EXTRA_DIST = $(TXT) $(HTML) $(SGML)
	
all-local: $(TXT) $(HTML)

clean-local:
	rm -f $(TXT) $(HTML)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	for i in $(TXT) $(HTML); do \
	  echo '-- Installing'$$i ; \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)/$$i; \
	done
