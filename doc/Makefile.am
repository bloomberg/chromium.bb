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
	
if ENABLE_DOCS
DOCS=$(TXT) $(HTML)
all-local: all-docs
clean-local: clean-docs
install-data-local: install-docs
else
all-local:
clean-local:
install-data-local:
endif

all-docs: $(DOCS)

clean-docs:
	rm -f $(DOCS)

install-docs:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	for i in $(DOCS); do \
	  echo '-- Installing'$$i ; \
	  $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)/$$i; \
	done
