COVERAGE_LIMIT=50

check:
	python ../../third_party/pycoverage run ./test_all.py
	python ../../third_party/pycoverage report --fail-under=$(COVERAGE_LIMIT)

.PHONY: check
